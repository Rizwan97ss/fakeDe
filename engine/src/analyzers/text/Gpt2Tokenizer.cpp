#include "Gpt2Tokenizer.h"

#include <nlohmann/json.hpp>

#include <climits>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fakede {

namespace {

std::string utf8Encode(char32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

// Decodes UTF-8 text into codepoints plus each codepoint's starting byte offset (with
// a trailing sentinel == text.size()), so callers can recover exact byte substrings
// for any codepoint range - needed because BPE operates on raw UTF-8 bytes, not on
// decoded codepoints.
struct Decoded {
    std::vector<char32_t> codepoints;
    std::vector<size_t> byteOffsets;
};

Decoded utf8Decode(const std::string& text) {
    Decoded d;
    size_t i = 0;
    while (i < text.size()) {
        d.byteOffsets.push_back(i);
        const unsigned char c0 = static_cast<unsigned char>(text[i]);
        if (c0 < 0x80) {
            d.codepoints.push_back(c0);
            i += 1;
        } else if ((c0 & 0xE0) == 0xC0 && i + 1 < text.size()) {
            const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
            d.codepoints.push_back(static_cast<char32_t>(((c0 & 0x1F) << 6) | (c1 & 0x3F)));
            i += 2;
        } else if ((c0 & 0xF0) == 0xE0 && i + 2 < text.size()) {
            const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
            const unsigned char c2 = static_cast<unsigned char>(text[i + 2]);
            d.codepoints.push_back(static_cast<char32_t>(((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F)));
            i += 3;
        } else if ((c0 & 0xF8) == 0xF0 && i + 3 < text.size()) {
            const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
            const unsigned char c2 = static_cast<unsigned char>(text[i + 2]);
            const unsigned char c3 = static_cast<unsigned char>(text[i + 3]);
            d.codepoints.push_back(
                static_cast<char32_t>(((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F)));
            i += 4;
        } else {
            // Invalid/unsupported lead byte: fall back to treating it as one raw byte
            // rather than throwing - defensive, not expected on well-formed input.
            d.codepoints.push_back(c0);
            i += 1;
        }
    }
    d.byteOffsets.push_back(text.size());
    return d;
}

enum class CharClass { Space, Letter, Digit, Other };

CharClass classify(char32_t cp) {
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == '\v') return CharClass::Space;
    if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) return CharClass::Letter;
    if (cp >= '0' && cp <= '9') return CharClass::Digit;
    // Basic Latin-1 letter ranges (covers common accented Western-European text).
    // Full \p{L} Unicode-category matching would need ICU; out of scope here - see
    // the class comment on why exactness matters for the *merge* step (handled
    // correctly below) but is a reasonable, documented approximation for splitting.
    if ((cp >= 0xC0 && cp <= 0xD6) || (cp >= 0xD8 && cp <= 0xF6) || (cp >= 0xF8 && cp <= 0xFF)) return CharClass::Letter;
    return CharClass::Other;
}

bool matchContraction(const std::vector<char32_t>& cps, size_t i, size_t& matchLen) {
    static const std::vector<std::u32string> kSuffixes = {
        U"'s", U"'t", U"'re", U"'ve", U"'m", U"'ll", U"'d",
    };
    if (cps[i] != '\'') return false;
    for (const auto& suffix : kSuffixes) {
        if (i + suffix.size() > cps.size()) continue;
        bool matches = true;
        for (size_t k = 0; k < suffix.size(); ++k) {
            if (cps[i + k] != suffix[k]) { matches = false; break; }
        }
        if (matches) { matchLen = suffix.size(); return true; }
    }
    return false;
}

} // namespace

Gpt2Tokenizer::Gpt2Tokenizer(const std::string& vocabPath, const std::string& mergesPath) {
    if (!std::filesystem::exists(vocabPath) || !std::filesystem::exists(mergesPath)) {
        return;
    }

    std::ifstream vocabFile(vocabPath);
    nlohmann::json vocabJson;
    vocabFile >> vocabJson;
    for (auto it = vocabJson.begin(); it != vocabJson.end(); ++it) {
        vocab_[it.key()] = it.value().get<int64_t>();
    }

    std::ifstream mergesFile(mergesPath);
    std::string line;
    int rank = 0;
    bool first = true;
    while (std::getline(mergesFile, line)) {
        if (first) {
            first = false;
            if (!line.empty() && line[0] == '#') continue; // header line, e.g. "#version: 0.2"
        }
        if (line.empty()) continue;
        mergeRank_[line] = rank++;
    }

    // bytes_to_unicode(), matching OpenAI's original gpt2 encoder.py exactly: maps
    // every byte value to a printable-range codepoint so BPE never has to deal with
    // control characters or whitespace as raw merge symbols.
    std::vector<int> bs;
    for (int b = static_cast<int>('!'); b <= static_cast<int>('~'); ++b) bs.push_back(b);
    for (int b = 0xA1; b <= 0xAC; ++b) bs.push_back(b);
    for (int b = 0xAE; b <= 0xFF; ++b) bs.push_back(b);
    std::vector<bool> inBs(256, false);
    for (int b : bs) inBs[b] = true;
    std::vector<int> cs = bs;
    int n = 0;
    for (int b = 0; b < 256; ++b) {
        if (!inBs[b]) {
            bs.push_back(b);
            cs.push_back(256 + n);
            ++n;
        }
    }
    for (size_t i = 0; i < bs.size(); ++i) {
        byteToUnicode_[static_cast<uint8_t>(bs[i])] = static_cast<char32_t>(cs[i]);
    }

    loaded_ = !vocab_.empty() && !mergeRank_.empty();
}

std::vector<std::string> Gpt2Tokenizer::preTokenize(const std::string& text) const {
    const Decoded d = utf8Decode(text);
    const auto& cps = d.codepoints;
    const size_t n = cps.size();
    std::vector<std::string> pieces;

    auto emitRange = [&](size_t startCp, size_t endCp) {
        if (startCp >= endCp) return;
        pieces.push_back(text.substr(d.byteOffsets[startCp], d.byteOffsets[endCp] - d.byteOffsets[startCp]));
    };

    size_t i = 0;
    while (i < n) {
        size_t contractionLen = 0;
        if (matchContraction(cps, i, contractionLen)) {
            emitRange(i, i + contractionLen);
            i += contractionLen;
            continue;
        }

        const bool hasLeadingSpace = (cps[i] == ' ');
        const size_t contentStart = hasLeadingSpace ? i + 1 : i;
        if (contentStart < n && classify(cps[contentStart]) != CharClass::Space) {
            const CharClass cls = classify(cps[contentStart]);
            size_t k = contentStart;
            while (k < n && classify(cps[k]) == cls) ++k;
            emitRange(i, k);
            i = k;
            continue;
        }

        // Whitespace run: emit all but the last space as one token (so the final
        // space can become the next token's leading space), except at end-of-text
        // where the whole run is emitted together. Single-space runs before content
        // never reach here - they're consumed by the branch above.
        if (classify(cps[i]) == CharClass::Space) {
            size_t k = i;
            while (k < n && classify(cps[k]) == CharClass::Space) ++k;
            const size_t runLen = k - i;
            if (k == n || runLen == 1) {
                emitRange(i, k);
                i = k;
            } else {
                emitRange(i, k - 1);
                i = k - 1;
            }
            continue;
        }

        // Shouldn't be reachable (every class is handled above), but never loop forever.
        emitRange(i, i + 1);
        ++i;
    }

    return pieces;
}

std::vector<std::string> Gpt2Tokenizer::bpe(const std::string& token) const {
    std::vector<std::string> symbols;
    symbols.reserve(token.size());
    for (unsigned char b : token) {
        symbols.push_back(utf8Encode(byteToUnicode_.at(b)));
    }
    if (symbols.size() < 2) return symbols;

    while (true) {
        int bestRank = INT_MAX;
        size_t bestIdx = SIZE_MAX;
        for (size_t k = 0; k + 1 < symbols.size(); ++k) {
            const auto it = mergeRank_.find(symbols[k] + " " + symbols[k + 1]);
            if (it != mergeRank_.end() && it->second < bestRank) {
                bestRank = it->second;
                bestIdx = k;
            }
        }
        if (bestIdx == SIZE_MAX) break;

        std::vector<std::string> merged;
        merged.reserve(symbols.size() - 1);
        for (size_t k = 0; k < symbols.size();) {
            if (k == bestIdx) {
                merged.push_back(symbols[k] + symbols[k + 1]);
                k += 2;
            } else {
                merged.push_back(symbols[k]);
                k += 1;
            }
        }
        symbols = std::move(merged);
    }
    return symbols;
}

std::vector<int64_t> Gpt2Tokenizer::encode(const std::string& text) const {
    std::vector<int64_t> ids;
    if (!loaded_) return ids;
    for (const auto& piece : preTokenize(text)) {
        for (const auto& symbol : bpe(piece)) {
            const auto it = vocab_.find(symbol);
            if (it != vocab_.end()) ids.push_back(it->second);
        }
    }
    return ids;
}

} // namespace fakede
