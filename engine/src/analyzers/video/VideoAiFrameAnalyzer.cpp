#include "VideoAiFrameAnalyzer.h"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <numeric>
#include <sstream>

#include "analyzers/image/AiGeneratedModelAnalyzer.h"
#include "util/VideoFrameSampler.h"

namespace fakede {

namespace {
constexpr int kMaxFrames = 6; // fewer than the classical analyzer - ONNX inference per frame is the expensive step
constexpr size_t kMinFrames = 2;

AnalysisInput frameToPngInput(const cv::Mat& frame) {
    AnalysisInput input;
    input.fileName = "frame.png";
    input.mimeType = "image/png";
    cv::imencode(".png", frame, input.bytes);
    return input;
}
} // namespace

VideoAiFrameAnalyzer::VideoAiFrameAnalyzer(const AiGeneratedModelAnalyzer* imageClassifier)
    : imageClassifier_(imageClassifier) {}

bool VideoAiFrameAnalyzer::isAvailable() const { return imageClassifier_->isAvailable(); }

Evidence VideoAiFrameAnalyzer::analyze(const AnalysisInput& input) const {
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

    std::vector<double> frameScores;
    for (const auto& frame : sample->frames) {
        const AnalysisInput frameInput = frameToPngInput(frame);
        frameScores.push_back(imageClassifier_->analyze(frameInput).score);
    }

    const double meanScore = std::accumulate(frameScores.begin(), frameScores.end(), 0.0) / frameScores.size();
    double sqSum = 0.0;
    for (double s : frameScores) sqSum += (s - meanScore) * (s - meanScore);
    const double stdScore = frameScores.size() > 1 ? std::sqrt(sqSum / frameScores.size()) : 0.0;

    evidence.score = meanScore;
    // High standalone confidence when it runs (purpose-built classifier, same as the
    // still-image case), but scaled down slightly for small sample sizes.
    evidence.confidence = std::clamp(0.5 + 0.3 * (frameScores.size() / static_cast<double>(kMaxFrames)), 0.0, 0.8);

    std::ostringstream explanation;
    explanation << "We checked " << frameScores.size() << " frames from this video using our AI-image "
                << "detector. On average, the frames look like "
                << (meanScore > 0.6
                        ? "they were AI-generated rather than real camera footage."
                        : "real camera footage, not AI-generated. Note: this check is known to be less "
                          "reliable against newer AI video/image tools, so this reading is weaker evidence "
                          "than a \"looks AI-made\" one would be.");
    evidence.explanation = explanation.str();
    evidence.rawDetails["sampledFrames"] = frameScores.size();
    evidence.rawDetails["frameScoreStd"] = stdScore;

    return evidence;
}

} // namespace fakede
