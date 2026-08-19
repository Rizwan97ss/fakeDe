#include "TextPerplexityAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace fakede {

namespace {
constexpr size_t kMaxTokens = 256; // bounds inference cost; long documents are sampled, not fully scored
} // namespace

TextPerplexityAnalyzer::TextPerplexityAnalyzer(const std::string& modelPath, const std::string& vocabPath,
                                                const std::string& mergesPath)
    : session_(std::make_unique<OnnxSession>(modelPath)),
      tokenizer_(std::make_unique<Gpt2Tokenizer>(vocabPath, mergesPath)) {}

Evidence TextPerplexityAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    if (!isAvailable()) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "Language model is not loaded (weights not fetched).";
        return evidence;
    }

    const std::string text(input.bytes.begin(), input.bytes.end());
    std::vector<int64_t> ids = tokenizer_->encode(text);
    if (ids.size() > kMaxTokens) ids.resize(kMaxTokens);

    if (ids.size() < 8) {
        evidence.score = 0.5;
        evidence.confidence = 0.05;
        evidence.explanation = "Text is too short for language-model perplexity scoring.";
        return evidence;
    }

    const int64_t seqLen = static_cast<int64_t>(ids.size());
    const std::vector<int64_t> shape{1, seqLen};
    const auto output = session_->runInt64InputFloatOutput(ids, shape);
    if (!output || output->empty()) {
        evidence.score = 0.5;
        evidence.confidence = 0.05;
        evidence.explanation = "Language model inference failed.";
        return evidence;
    }

    const int64_t vocabSize = static_cast<int64_t>(output->size()) / seqLen;
    if (vocabSize <= 0) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "Unexpected model output shape.";
        return evidence;
    }

    // Per-position log P(actual next token), via causal next-token prediction: logits
    // at position p predict the token at p+1.
    std::vector<double> surprisals;
    surprisals.reserve(seqLen - 1);
    for (int64_t pos = 0; pos + 1 < seqLen; ++pos) {
        const float* logits = output->data() + pos * vocabSize;
        float maxLogit = logits[0];
        for (int64_t v = 1; v < vocabSize; ++v) maxLogit = std::max(maxLogit, logits[v]);
        double sumExp = 0.0;
        for (int64_t v = 0; v < vocabSize; ++v) sumExp += std::exp(static_cast<double>(logits[v] - maxLogit));

        const int64_t actualNext = ids[static_cast<size_t>(pos + 1)];
        const double logProb = static_cast<double>(logits[actualNext] - maxLogit) - std::log(sumExp);
        surprisals.push_back(-logProb);
    }

    const double meanSurprisal = std::accumulate(surprisals.begin(), surprisals.end(), 0.0) / surprisals.size();
    double sqSum = 0.0;
    for (double s : surprisals) sqSum += (s - meanSurprisal) * (s - meanSurprisal);
    const double stdSurprisal = std::sqrt(sqSum / surprisals.size());
    const double perplexity = std::exp(meanSurprisal);
    const double burstiness = meanSurprisal > 0.0 ? stdSurprisal / meanSurprisal : 0.0;

    // Provisional, reasoned thresholds - not statistically calibrated (see
    // FusionEngine.h for the project-wide stance on why not yet). Lower perplexity and
    // lower burstiness both push toward "unusually predictable to GPT-2".
    const double perplexityScore = std::clamp(1.0 - (perplexity - 10.0) / 60.0, 0.0, 1.0);
    const double burstinessScore = std::clamp(1.0 - burstiness / 1.2, 0.0, 1.0);
    const double score = std::clamp(0.6 * perplexityScore + 0.4 * burstinessScore, 0.0, 1.0);

    const double lengthFactor = std::clamp(static_cast<double>(ids.size()) / 200.0, 0.0, 1.0);
    const double confidence = 0.15 + 0.25 * lengthFactor; // capped low by design, see class comment

    evidence.score = score;
    evidence.confidence = confidence;
    std::ostringstream explanation;
    explanation << "GPT-2 perplexity " << std::fixed << std::setprecision(1) << perplexity << " - "
                << (score > 0.6 ? "text is unusually predictable to the model, one weak signal among several "
                                  "consistent with AI generation."
                                : "text's predictability is within a typical human-writing range.");
    evidence.explanation = explanation.str();
    evidence.rawDetails["perplexity"] = perplexity;
    evidence.rawDetails["burstiness"] = burstiness;
    evidence.rawDetails["tokenCount"] = ids.size();

    return evidence;
}

} // namespace fakede
