#include "AiGeneratedModelAnalyzer.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace fakede {

namespace {
constexpr int kInputSize = 224;
// CLIP/OpenAI normalization constants, matching UniversalFakeDetect's CLIP ViT-L/14
// backbone. If a different backbone is used, update these alongside the model in
// docs/model-sourcing.md.
constexpr float kMean[3] = {0.48145466f, 0.4578275f, 0.40821073f};
constexpr float kStd[3] = {0.26862954f, 0.26130258f, 0.27577711f};

float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }
} // namespace

AiGeneratedModelAnalyzer::AiGeneratedModelAnalyzer(const std::string& modelPath)
    : session_(std::make_unique<OnnxSession>(modelPath)) {}

std::vector<std::string> AiGeneratedModelAnalyzer::supportedMimeTypes() const {
    return {"image/jpeg", "image/png", "image/webp", "image/bmp"};
}

Evidence AiGeneratedModelAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    if (!session_->isLoaded()) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "The AI-detection model isn't set up on this server yet, so this check was skipped.";
        return evidence;
    }

    cv::Mat bgr = cv::imdecode(input.bytes, cv::IMREAD_COLOR);
    if (bgr.empty()) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "We couldn't open this image to check it.";
        return evidence;
    }

    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(kInputSize, kInputSize), 0, 0, cv::INTER_AREA);
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32FC3, 1.0 / 255.0);

    // HWC -> CHW, normalized.
    std::vector<float> tensorData(3 * kInputSize * kInputSize);
    for (int c = 0; c < 3; ++c) {
        for (int y = 0; y < kInputSize; ++y) {
            const cv::Vec3f* row = rgb.ptr<cv::Vec3f>(y);
            for (int x = 0; x < kInputSize; ++x) {
                const float v = (row[x][c] - kMean[c]) / kStd[c];
                tensorData[c * kInputSize * kInputSize + y * kInputSize + x] = v;
            }
        }
    }

    const std::vector<int64_t> shape{1, 3, kInputSize, kInputSize};
    auto output = session_->runSingleInputOutput(tensorData, shape);
    if (!output || output->empty()) {
        evidence.score = 0.5;
        evidence.confidence = 0.1;
        evidence.explanation = "This check didn't run properly on this image.";
        return evidence;
    }

    double fakeProbability = 0.5;
    if (output->size() == 1) {
        fakeProbability = sigmoid((*output)[0]);
    } else {
        // Softmax over 2 (or more) classes, take the probability mass on every class
        // after index 0 as "not authentic" — covers both binary and multi-class exports.
        float maxLogit = *std::max_element(output->begin(), output->end());
        double sumExp = 0.0;
        for (float v : *output) sumExp += std::exp(v - maxLogit);
        double authenticProb = std::exp((*output)[0] - maxLogit) / sumExp;
        fakeProbability = 1.0 - authenticProb;
    }

    evidence.score = std::clamp(fakeProbability, 0.0, 1.0);
    evidence.confidence = 0.85; // purpose-built classifier; high standalone confidence when loaded
    evidence.explanation = evidence.score > 0.6
        ? "We ran this image through a model trained to recognize AI-generated pictures (from tools like "
          "Midjourney, DALL-E, or Stable Diffusion). It thinks this image was very likely made by AI."
        : "We ran this image through a model trained to recognize AI-generated pictures. It thinks this "
          "image was very likely a real photo, not AI-made.";
    evidence.rawDetails["fakeProbability"] = fakeProbability;

    return evidence;
}

} // namespace fakede
