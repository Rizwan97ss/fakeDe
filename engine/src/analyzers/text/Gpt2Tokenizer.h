#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace fakede {

// A from-scratch implementation of GPT-2's byte-level BPE tokenizer, matching OpenAI's
// original algorithm (byte->unicode remapping, then iterative pair-merging by rank).
// This exists because using GPT-2's pretrained embedding weights with anything less
// than the exact tokenization they were trained on produces meaningless token ids -
// the model would silently emit garbage probabilities that *look* like a real
// perplexity score. An approximate/simplified tokenizer would be worse than no
// tokenizer at all here, so this is a full implementation, not a shortcut.
//
// Needs vocab.json and merges.txt (GPT-2's original release files) at construction;
// isLoaded() is false if they're missing, so TextPerplexityAnalyzer can degrade
// gracefully like every other model-backed analyzer in this codebase.
class Gpt2Tokenizer {
public:
    Gpt2Tokenizer(const std::string& vocabPath, const std::string& mergesPath);

    bool isLoaded() const { return loaded_; }

    // Returns GPT-2 token ids for the input text. Unknown byte sequences never occur
    // by construction (every byte has a mapped symbol and single-symbol tokens are
    // always in the base vocab), so this never needs an <unk> fallback.
    std::vector<int64_t> encode(const std::string& text) const;

private:
    bool loaded_ = false;
    std::unordered_map<std::string, int64_t> vocab_;          // BPE token (as UTF-8) -> id
    std::unordered_map<std::string, int> mergeRank_;          // "left right" -> priority (lower = merge first)
    std::unordered_map<uint8_t, char32_t> byteToUnicode_;

    std::vector<std::string> preTokenize(const std::string& text) const;
    std::vector<std::string> bpe(const std::string& token) const;
};

} // namespace fakede
