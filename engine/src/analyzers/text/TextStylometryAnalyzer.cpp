#include "TextStylometryAnalyzer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <set>
#include <sstream>
#include <vector>

namespace fakede {

namespace {

std::vector<std::string> splitWords(const std::string& text) {
    std::vector<std::string> words;
    std::string current;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '\'') {
            current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) words.push_back(current);
    return words;
}

// Splits on '.', '!', '?' followed by whitespace or end-of-text. A heuristic, not a
// real sentence boundary detector (doesn't special-case "Mr.", decimals, etc.) -
// adequate for a statistical signal, not for anything requiring precision.
std::vector<std::string> splitSentences(const std::string& text) {
    std::vector<std::string> sentences;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i) {
        current.push_back(text[i]);
        const char c = text[i];
        if (c == '.' || c == '!' || c == '?') {
            const bool atEnd = (i + 1 == text.size());
            const bool followedByWhitespace = !atEnd && std::isspace(static_cast<unsigned char>(text[i + 1]));
            if (atEnd || followedByWhitespace) {
                sentences.push_back(current);
                current.clear();
            }
        }
    }
    if (!current.empty()) sentences.push_back(current);
    return sentences;
}

double mean(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

double stddev(const std::vector<double>& v, double m) {
    if (v.size() < 2) return 0.0;
    double sq = 0.0;
    for (double x : v) sq += (x - m) * (x - m);
    return std::sqrt(sq / (v.size() - 1));
}

} // namespace

Evidence TextStylometryAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    const std::string text(input.bytes.begin(), input.bytes.end());
    const std::vector<std::string> sentences = splitSentences(text);
    const std::vector<std::string> allWords = splitWords(text);

    if (sentences.size() < 4 || allWords.size() < 40) {
        evidence.score = 0.5;
        evidence.confidence = 0.05;
        evidence.explanation = "Text is too short for stylometric analysis to be meaningful.";
        return evidence;
    }

    std::vector<double> sentenceWordCounts;
    sentenceWordCounts.reserve(sentences.size());
    for (const auto& s : sentences) {
        sentenceWordCounts.push_back(static_cast<double>(splitWords(s).size()));
    }
    const double sentMean = mean(sentenceWordCounts);
    const double sentStd = stddev(sentenceWordCounts, sentMean);
    const double burstiness = sentMean > 0.0 ? sentStd / sentMean : 0.0;

    // Human writing tends toward more varied ("bursty") sentence length; typical AI
    // output historically trends more uniform. Documented pattern (GPTZero's original
    // perplexity+burstiness approach), not a hard rule for modern instruction-tuned
    // models, which is exactly why this stays low-confidence.
    const double burstinessScore = std::clamp(1.0 - burstiness / 0.6, 0.0, 1.0);

    std::set<std::string> uniqueWords(allWords.begin(), allWords.end());
    const double herdanC = std::log(static_cast<double>(uniqueWords.size())) / std::log(static_cast<double>(allWords.size()));
    // Herdan's C for natural English prose of this length is typically ~0.65-0.8; this
    // is a weak, two-sided signal (either extreme is mildly suspicious), so it's kept
    // close to neutral and only nudges the score a little.
    const double vocabDeviationScore = std::clamp(std::abs(herdanC - 0.72) / 0.15, 0.0, 1.0);

    // Repeated-trigram rate: fraction of word-trigrams that occur more than once.
    std::vector<std::string> trigrams;
    for (size_t i = 0; i + 2 < allWords.size(); ++i) {
        trigrams.push_back(allWords[i] + " " + allWords[i + 1] + " " + allWords[i + 2]);
    }
    double repetitionScore = 0.0;
    if (!trigrams.empty()) {
        std::set<std::string> uniqueTrigrams(trigrams.begin(), trigrams.end());
        const double repeatedFraction = 1.0 - static_cast<double>(uniqueTrigrams.size()) / trigrams.size();
        repetitionScore = std::clamp(repeatedFraction / 0.15, 0.0, 1.0);
    }

    const double score = std::clamp(0.6 * burstinessScore + 0.2 * vocabDeviationScore + 0.2 * repetitionScore, 0.0, 1.0);

    // Confidence scales gently with sample size but is hard-capped: text stylometry is
    // never strong standalone evidence, by design (see docs/model-sourcing.md).
    const double lengthFactor = std::clamp(allWords.size() / 400.0, 0.0, 1.0);
    const double confidence = 0.15 + 0.25 * lengthFactor;

    evidence.score = score;
    evidence.confidence = confidence;
    std::ostringstream explanation;
    explanation << "Sentence-length variation is " << (burstiness < 0.4 ? "low (more uniform than typical human writing)"
                                                                          : "consistent with typical human writing")
                << ". Text stylometry alone is weak evidence - treat as advisory.";
    evidence.explanation = explanation.str();
    evidence.rawDetails["sentenceCount"] = sentences.size();
    evidence.rawDetails["wordCount"] = allWords.size();
    evidence.rawDetails["burstiness"] = burstiness;
    evidence.rawDetails["herdanC"] = herdanC;

    return evidence;
}

} // namespace fakede
