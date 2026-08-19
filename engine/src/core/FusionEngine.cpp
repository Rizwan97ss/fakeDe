#include "FusionEngine.h"

#include <algorithm>
#include <cmath>

namespace fakede {

namespace {
constexpr double kAuthenticThreshold = 0.35;
constexpr double kFakeThreshold = 0.65;
// Below this fused confidence there wasn't enough usable signal (e.g. most analyzers
// were unavailable) to say anything stronger than "inconclusive".
constexpr double kMinUsableConfidence = 0.15;
} // namespace

double FusionEngine::weightFor(const std::string& analyzerId) const {
    if (analyzerId.rfind("ai-model:", 0) == 0) return 1.5;  // purpose-built classifier
    if (analyzerId == "metadata") return 0.8;                // easy to strip/forge, weight down
    if (analyzerId == "noise-residual") return 0.9;
    return 1.0; // "ela", "freq-fft", and anything unlisted
}

Verdict FusionEngine::fuse(std::vector<Evidence> evidence) const {
    Verdict verdict;
    verdict.evidenceBreakdown = evidence;

    double weightedScoreSum = 0.0;
    double weightedConfidenceSum = 0.0; // denominator for the score average
    double confidenceSum = 0.0;         // for the overall confidence average

    for (const auto& e : verdict.evidenceBreakdown) {
        const double w = weightFor(e.analyzerId) * e.confidence;
        weightedScoreSum += e.score * w;
        weightedConfidenceSum += w;
        confidenceSum += e.confidence;
    }

    if (weightedConfidenceSum > 0.0) {
        verdict.overallScore = weightedScoreSum / weightedConfidenceSum;
    } else {
        verdict.overallScore = 0.5; // no usable signal at all
    }

    verdict.overallConfidence =
        verdict.evidenceBreakdown.empty() ? 0.0 : confidenceSum / static_cast<double>(verdict.evidenceBreakdown.size());

    if (verdict.overallConfidence < kMinUsableConfidence) {
        verdict.overallLabel = VerdictLabel::Inconclusive;
    } else if (verdict.overallScore < kAuthenticThreshold) {
        verdict.overallLabel = VerdictLabel::LikelyAuthentic;
    } else if (verdict.overallScore > kFakeThreshold) {
        verdict.overallLabel = VerdictLabel::LikelyAiGeneratedOrAltered;
    } else {
        verdict.overallLabel = VerdictLabel::Inconclusive;
    }

    return verdict;
}

} // namespace fakede
