#include "NoiseResidualAnalyzer.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace fakede {

namespace {

// M_PI isn't standard C++ (MSVC only defines it with _USE_MATH_DEFINES), so spell it
// out rather than depend on a compiler extension.
constexpr double kPi = 3.14159265358979323846;

// Fast noise-sigma estimator (Immerkaer 1996): convolve with a Laplacian-of-the-form
// kernel that has zero response on smooth/linear regions, so its output is driven
// almost entirely by noise rather than image content.
double estimateNoiseSigma(const cv::Mat& grayFloat) {
    static const cv::Mat kernel = (cv::Mat_<float>(3, 3) << 1, -2, 1, -2, 4, -2, 1, -2, 1);
    cv::Mat conv;
    cv::filter2D(grayFloat, conv, CV_32F, kernel);
    const double sumAbs = cv::sum(cv::abs(conv))[0];
    const int w = grayFloat.cols, h = grayFloat.rows;
    if (w <= 2 || h <= 2) return 0.0;
    return std::sqrt(kPi / 2.0) * sumAbs / (6.0 * (w - 2) * (h - 2));
}

} // namespace

std::vector<std::string> NoiseResidualAnalyzer::supportedMimeTypes() const {
    return {"image/jpeg", "image/png", "image/webp", "image/bmp"};
}

Evidence NoiseResidualAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    cv::Mat bgr = cv::imdecode(input.bytes, cv::IMREAD_COLOR);
    if (bgr.empty() || std::min(bgr.rows, bgr.cols) < 64) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "This image is too small, or couldn't be opened, to check its grain and texture.";
        return evidence;
    }

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    gray.convertTo(gray, CV_32F);

    const double globalSigma = estimateNoiseSigma(gray);

    // Per-block estimate on a coarse grid to look for patchy/inconsistent noise, a
    // splicing tell (a pasted region rarely matches the host image's noise floor).
    constexpr int kGrid = 6;
    const int blockH = gray.rows / kGrid;
    const int blockW = gray.cols / kGrid;
    std::vector<double> blockSigmas;
    if (blockH >= 16 && blockW >= 16) {
        for (int by = 0; by < kGrid; ++by) {
            for (int bx = 0; bx < kGrid; ++bx) {
                cv::Rect roi(bx * blockW, by * blockH, blockW, blockH);
                blockSigmas.push_back(estimateNoiseSigma(gray(roi)));
            }
        }
    }

    double blockMean = 0.0, blockStd = 0.0;
    if (!blockSigmas.empty()) {
        blockMean = std::accumulate(blockSigmas.begin(), blockSigmas.end(), 0.0) / blockSigmas.size();
        double sqSum = 0.0;
        for (double s : blockSigmas) sqSum += (s - blockMean) * (s - blockMean);
        blockStd = std::sqrt(sqSum / blockSigmas.size());
    }
    const double coefficientOfVariation = blockMean > 1e-6 ? blockStd / blockMean : 0.0;

    // Unnaturally smooth (very low global noise) pushes toward "synthetic"; patchy,
    // high-variance noise across blocks pushes toward "spliced/altered".
    const double lowNoiseScore = std::clamp(1.0 - globalSigma / 6.0, 0.0, 1.0);
    const double inconsistencyScore = std::clamp(coefficientOfVariation / 1.5, 0.0, 1.0);
    const double score = std::clamp(0.5 * lowNoiseScore + 0.5 * inconsistencyScore, 0.0, 1.0);

    evidence.score = score;
    evidence.confidence = blockSigmas.empty() ? 0.3 : 0.45;
    evidence.explanation = lowNoiseScore > inconsistencyScore
        ? (score > 0.55 ? "This image is unusually smooth all over — real camera photos almost always have a "
                          "little natural grain that this one is missing, which is common in AI-made images."
                        : "This image has the small amount of natural grain you'd expect from a real camera.")
        : (score > 0.55 ? "Different parts of this image have noticeably different amounts of grain, which "
                          "can happen when pieces from different photos are combined."
                        : "The grain in this image looks consistent from one part to another.");
    evidence.rawDetails["globalNoiseSigma"] = globalSigma;
    evidence.rawDetails["blockNoiseCoefficientOfVariation"] = coefficientOfVariation;

    return evidence;
}

} // namespace fakede
