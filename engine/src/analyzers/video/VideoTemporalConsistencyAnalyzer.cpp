#include "VideoTemporalConsistencyAnalyzer.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

#include "analyzers/image/ElaAnalyzer.h"
#include "analyzers/image/FrequencyAnalyzer.h"
#include "analyzers/image/NoiseResidualAnalyzer.h"
#include "util/VideoFrameSampler.h"

namespace fakede {

namespace {
constexpr int kMaxFrames = 8;
constexpr size_t kMinFrames = 3;

double stddev(const std::vector<double>& v) {
    if (v.size() < 2) return 0.0;
    const double m = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    double sq = 0.0;
    for (double x : v) sq += (x - m) * (x - m);
    return std::sqrt(sq / v.size());
}

AnalysisInput frameToPngInput(const cv::Mat& frame) {
    AnalysisInput input;
    input.fileName = "frame.png";
    input.mimeType = "image/png";
    cv::imencode(".png", frame, input.bytes);
    return input;
}
} // namespace

VideoTemporalConsistencyAnalyzer::VideoTemporalConsistencyAnalyzer(const ElaAnalyzer* ela, const FrequencyAnalyzer* freq,
                                                                     const NoiseResidualAnalyzer* noise)
    : ela_(ela), freq_(freq), noise_(noise) {}

Evidence VideoTemporalConsistencyAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    const auto sample = VideoFrameSampler::sample(input.bytes, kMaxFrames);
    if (!sample || sample->frames.size() < kMinFrames) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "We couldn't process enough of this video to check it.";
        return evidence;
    }

    std::vector<double> elaScores, freqScores, noiseScores;
    for (const auto& frame : sample->frames) {
        const AnalysisInput frameInput = frameToPngInput(frame);
        elaScores.push_back(ela_->analyze(frameInput).score);
        freqScores.push_back(freq_->analyze(frameInput).score);
        noiseScores.push_back(noise_->analyze(frameInput).score);
    }

    const double elaStd = stddev(elaScores);
    const double freqStd = stddev(freqScores);
    const double noiseStd = stddev(noiseScores);

    // Provisional, reasoned threshold (see FusionEngine.h for the project-wide stance
    // on why not statistically calibrated yet). Higher frame-to-frame variance in
    // these classical scores -> more inconsistent -> more suspicious.
    const double score = std::clamp((elaStd + freqStd + noiseStd) / 3.0 / 0.25, 0.0, 1.0);
    const double confidence =
        std::clamp(0.2 + 0.3 * (sample->frames.size() / static_cast<double>(kMaxFrames)), 0.0, 0.5);

    evidence.score = score;
    evidence.confidence = confidence;
    std::ostringstream explanation;
    explanation << "We checked " << sample->frames.size() << " frames from this video. "
                << (score > 0.5 ? "The picture-quality clues changed noticeably from frame to frame, which can "
                                  "happen with edited or AI-generated video rather than one continuous "
                                  "camera recording."
                                : "The picture-quality clues stayed fairly consistent frame to frame, like "
                                  "continuous footage from one camera.");
    evidence.explanation = explanation.str();
    evidence.rawDetails["sampledFrames"] = sample->frames.size();
    evidence.rawDetails["elaScoreStd"] = elaStd;
    evidence.rawDetails["freqScoreStd"] = freqStd;
    evidence.rawDetails["noiseScoreStd"] = noiseStd;

    return evidence;
}

} // namespace fakede
