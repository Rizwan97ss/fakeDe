#include "FusionEngine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fakede {

namespace {
constexpr double kAuthenticThreshold = 0.35;
constexpr double kFakeThreshold = 0.65;
// Below this fused confidence there wasn't enough usable signal (e.g. most analyzers
// were unavailable) to say anything stronger than "inconclusive". Also used as the
// per-analyzer cutoff for inclusion in the median/disagreement calculations below -
// an analyzer that reports itself as basically clueless shouldn't get an equal vote.
constexpr double kMinUsableConfidence = 0.15;

double median(std::vector<double> values) {
    if (values.empty()) return 0.5;
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    return (n % 2 == 1) ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

double stddev(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
    double sumSq = 0.0;
    for (const double v : values) sumSq += (v - mean) * (v - mean);
    return std::sqrt(sumSq / static_cast<double>(values.size()));
}
} // namespace

double FusionEngine::weightFor(const std::string& analyzerId) const {
    // A file's own embedded C2PA Content Credentials, when explicit, is the file
    // stating its own origin directly - stronger per-signal than a probabilistic
    // classifier's guess, so it outranks even the ai-model:* tier. Its own confidence
    // score already crashes to near-zero when no manifest is found (see
    // C2paManifestAnalyzer), so this high weight only ever matters when it actually
    // has something to say.
    if (analyzerId == "c2pa-manifest") return 1.7;
    if (analyzerId.rfind("ai-model:", 0) == 0) return 1.5;  // purpose-built classifier
    if (analyzerId == "metadata") return 0.8;                // easy to strip/forge, weight down
    if (analyzerId == "noise-residual") return 0.9;
    return 1.0; // "ela", "freq-fft", and anything unlisted
}

Verdict FusionEngine::fuse(std::vector<Evidence> evidence) const {
    Verdict verdict;
    verdict.evidenceBreakdown = evidence;

    if (verdict.evidenceBreakdown.empty()) {
        verdict.overallScore = 0.5;
        verdict.overallConfidence = 0.0;
        verdict.overallLabel = VerdictLabel::Inconclusive;
        return verdict;
    }

    double weightedScoreSum = 0.0;
    double weightedConfidenceSum = 0.0; // denominator for the score average
    double confidenceSum = 0.0;         // for the overall confidence average
    std::vector<double> usableScores;   // confidence >= kMinUsableConfidence only

    for (const auto& e : verdict.evidenceBreakdown) {
        const double w = weightFor(e.analyzerId) * e.confidence;
        weightedScoreSum += e.score * w;
        weightedConfidenceSum += w;
        confidenceSum += e.confidence;
        if (e.confidence >= kMinUsableConfidence) usableScores.push_back(e.score);
    }

    const double weightedAverage = weightedConfidenceSum > 0.0 ? weightedScoreSum / weightedConfidenceSum : 0.5;

    // Blend the confidence-weighted average with the plain median of usable scores.
    // The median is a robust anchor: unlike the weighted average, no single analyzer -
    // however confident it reports itself, however favorably weighted for being a
    // purpose-built ai-model:* classifier - can single-handedly outvote what the rest
    // of the evidence agrees on, because a median only moves when a majority of
    // *values* move, not when one value's weight grows. Falls back to the weighted
    // average alone when there's no usable score list (e.g. a single low-confidence
    // signal) to blend against.
    const double medianScore = usableScores.empty() ? weightedAverage : median(usableScores);
    verdict.overallScore = std::clamp(0.5 * weightedAverage + 0.5 * medianScore, 0.0, 1.0);

    const double meanConfidence = confidenceSum / static_cast<double>(verdict.evidenceBreakdown.size());

    // Dampen overall confidence when the usable evidence disagrees with itself: a wide
    // spread across scores means independent signals are telling different stories,
    // and reporting a flat, disagreement-blind confidence would overstate how sure
    // this verdict actually is. Agreement leaves confidence roughly as-is; conflict
    // honestly lowers it, capped at halving it so one noisy analyzer among many
    // agreeing ones can't tank confidence to zero.
    const double disagreementPenalty = std::clamp(stddev(usableScores) / 0.5, 0.0, 1.0);
    verdict.overallConfidence = meanConfidence * (1.0 - 0.5 * disagreementPenalty);

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
