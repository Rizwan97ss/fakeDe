#include "VoiceNaturalnessAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

#include "AudioFrameAnalysis.h"
#include "util/AudioDecoder.h"

namespace fakede {

namespace {
constexpr size_t kMinVoicedFrames = 10;
} // namespace

Evidence VoiceNaturalnessAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    const auto decoded = AudioDecoder::decode(input.bytes);
    if (!decoded) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "We couldn't process this audio.";
        return evidence;
    }

    const auto frames = analyzeFrames(decoded->monoSamples, decoded->sampleRate);

    std::vector<double> periods, amplitudes;
    for (const auto& f : frames) {
        if (f.period > 0.0) {
            periods.push_back(f.period);
            amplitudes.push_back(f.rms);
        }
    }

    if (periods.size() < kMinVoicedFrames) {
        evidence.score = 0.5;
        evidence.confidence = 0.05;
        evidence.explanation = "There isn't enough clear speech in this clip to check for natural voice variation.";
        return evidence;
    }

    double periodDiffSum = 0.0;
    for (size_t i = 1; i < periods.size(); ++i) periodDiffSum += std::abs(periods[i] - periods[i - 1]);
    const double meanPeriod = std::accumulate(periods.begin(), periods.end(), 0.0) / periods.size();
    const double jitter = meanPeriod > 0.0 ? (periodDiffSum / (periods.size() - 1)) / meanPeriod : 0.0;

    double ampDiffSum = 0.0;
    for (size_t i = 1; i < amplitudes.size(); ++i) ampDiffSum += std::abs(amplitudes[i] - amplitudes[i - 1]);
    const double meanAmp = std::accumulate(amplitudes.begin(), amplitudes.end(), 0.0) / amplitudes.size();
    const double shimmer = meanAmp > 0.0 ? (ampDiffSum / (amplitudes.size() - 1)) / meanAmp : 0.0;

    // Provisional, reasoned thresholds (see FusionEngine.h for the project-wide stance
    // on why these aren't statistically calibrated). Low jitter/shimmer -> unnaturally
    // smooth -> pushes toward "synthetic".
    const double jitterScore = std::clamp(1.0 - jitter / 0.05, 0.0, 1.0);
    const double shimmerScore = std::clamp(1.0 - shimmer / 0.15, 0.0, 1.0);
    const double score = std::clamp(0.5 * jitterScore + 0.5 * shimmerScore, 0.0, 1.0);

    const double lengthFactor = std::clamp(periods.size() / 100.0, 0.0, 1.0);
    const double confidence = 0.15 + 0.25 * lengthFactor; // capped low by design, see class comment

    evidence.score = score;
    evidence.confidence = confidence;
    std::ostringstream explanation;
    explanation << (score > 0.6
                         ? "This voice sounds unusually smooth and steady — real human voices naturally waver "
                           "a little more than this."
                         : "This voice has the small natural wavering you'd expect from a real human speaker.")
                << " This is a weaker clue against modern AI voice cloning, so treat it as advisory.";
    evidence.explanation = explanation.str();
    evidence.rawDetails["jitter"] = jitter;
    evidence.rawDetails["shimmer"] = shimmer;
    evidence.rawDetails["voicedFrameCount"] = periods.size();

    return evidence;
}

} // namespace fakede
