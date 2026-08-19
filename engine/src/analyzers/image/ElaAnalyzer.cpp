#include "ElaAnalyzer.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <vector>

#include "util/Base64.h"

namespace fakede {

namespace {
constexpr int kElaJpegQuality = 90;
constexpr int kElaAmplify = 12; // multiply the diff so faint artifacts are visible in the heatmap
} // namespace

std::vector<std::string> ElaAnalyzer::supportedMimeTypes() const {
    return {"image/jpeg", "image/png", "image/webp", "image/bmp"};
}

Evidence ElaAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    cv::Mat original = cv::imdecode(input.bytes, cv::IMREAD_COLOR);
    if (original.empty()) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "Image could not be decoded for Error Level Analysis.";
        return evidence;
    }

    std::vector<uint8_t> recompressedBytes;
    std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, kElaJpegQuality};
    cv::imencode(".jpg", original, recompressedBytes, params);
    cv::Mat recompressed = cv::imdecode(recompressedBytes, cv::IMREAD_COLOR);

    cv::Mat diff;
    cv::absdiff(original, recompressed, diff);
    diff.convertTo(diff, CV_8U, kElaAmplify);

    cv::Mat gray;
    cv::cvtColor(diff, gray, cv::COLOR_BGR2GRAY);

    cv::Scalar meanVal, stddevVal;
    cv::meanStdDev(gray, meanVal, stddevVal);
    double maxVal = 0.0;
    cv::minMaxLoc(gray, nullptr, &maxVal);

    // Heuristic: real, single-generation photos show low, fairly uniform ELA response.
    // Spliced/heavily-edited regions and fully-synthetic images with unnatural
    // compression histories show high mean and/or high local variance (hot spots).
    const double meanNorm = std::clamp(meanVal[0] / 40.0, 0.0, 1.0);
    const double maxNorm = std::clamp(maxVal / 220.0, 0.0, 1.0);
    const double score = std::clamp(0.5 * meanNorm + 0.5 * maxNorm, 0.0, 1.0);

    // Low resolution images are noisy for ELA; scale confidence with pixel count.
    const double megapixels = (original.rows * original.cols) / 1'000'000.0;
    const double confidence = std::clamp(0.35 + 0.25 * std::min(megapixels, 2.0), 0.0, 0.75);

    evidence.score = score;
    evidence.confidence = confidence;
    evidence.explanation = score > 0.6
        ? "Elevated and/or localized recompression error suggests possible splicing or editing."
        : "Recompression error is low and fairly uniform, consistent with a single-generation image.";

    cv::Mat heatmap;
    cv::applyColorMap(gray, heatmap, cv::COLORMAP_INFERNO);
    std::vector<uint8_t> pngBytes;
    cv::imencode(".png", heatmap, pngBytes);
    evidence.visualizationPngBase64 = base64Encode(pngBytes);
    evidence.rawDetails["meanError"] = meanVal[0];
    evidence.rawDetails["maxError"] = maxVal;

    return evidence;
}

} // namespace fakede
