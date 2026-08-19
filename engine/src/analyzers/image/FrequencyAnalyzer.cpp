#include "FrequencyAnalyzer.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "util/Base64.h"

namespace fakede {

namespace {

// Crops to a square power-of-two-ish region so the radial bins below are well-defined,
// and caps size for speed since FFT analysis doesn't need full resolution.
cv::Mat prepareGray(const cv::Mat& bgr) {
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    const int side = std::min({gray.rows, gray.cols, 512});
    cv::Rect roi((gray.cols - side) / 2, (gray.rows - side) / 2, side, side);
    cv::Mat cropped = gray(roi).clone();

    cropped.convertTo(cropped, CV_32F);
    return cropped;
}

void applyHannWindow(cv::Mat& img) {
    cv::Mat hann;
    cv::createHanningWindow(hann, img.size(), CV_32F);
    img = img.mul(hann);
}

} // namespace

std::vector<std::string> FrequencyAnalyzer::supportedMimeTypes() const {
    return {"image/jpeg", "image/png", "image/webp", "image/bmp"};
}

Evidence FrequencyAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    cv::Mat bgr = cv::imdecode(input.bytes, cv::IMREAD_COLOR);
    if (bgr.empty() || std::min(bgr.rows, bgr.cols) < 32) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "This image is too small, or couldn't be opened, to check for this kind of pattern.";
        return evidence;
    }

    cv::Mat gray = prepareGray(bgr);
    applyHannWindow(gray);

    cv::Mat planes[] = {gray, cv::Mat::zeros(gray.size(), CV_32F)};
    cv::Mat complexImg;
    cv::merge(planes, 2, complexImg);
    cv::dft(complexImg, complexImg);

    cv::split(complexImg, planes);
    cv::Mat magnitude;
    cv::magnitude(planes[0], planes[1], magnitude);
    magnitude += cv::Scalar::all(1);
    cv::log(magnitude, magnitude);

    // Reorder quadrants so the DC (zero-frequency) term sits at the center.
    const int cx = magnitude.cols / 2;
    const int cy = magnitude.rows / 2;
    cv::Mat q0(magnitude, cv::Rect(0, 0, cx, cy));
    cv::Mat q1(magnitude, cv::Rect(cx, 0, cx, cy));
    cv::Mat q2(magnitude, cv::Rect(0, cy, cx, cy));
    cv::Mat q3(magnitude, cv::Rect(cx, cy, cx, cy));
    cv::Mat tmp;
    q0.copyTo(tmp); q3.copyTo(q0); tmp.copyTo(q3);
    q1.copyTo(tmp); q2.copyTo(q1); tmp.copyTo(q2);

    // Radially average the log-magnitude spectrum into bins from center (low freq) to
    // corner (high freq), then compare the outer (high-frequency) band's energy and
    // peakiness against the smooth power-law falloff a natural photo exhibits.
    const int maxRadius = std::min(cx, cy);
    std::vector<double> radialSum(maxRadius, 0.0);
    std::vector<int> radialCount(maxRadius, 0);

    for (int y = 0; y < magnitude.rows; ++y) {
        const float* row = magnitude.ptr<float>(y);
        for (int x = 0; x < magnitude.cols; ++x) {
            const double dx = x - cx;
            const double dy = y - cy;
            const int r = static_cast<int>(std::sqrt(dx * dx + dy * dy));
            if (r >= 0 && r < maxRadius) {
                radialSum[r] += row[x];
                radialCount[r] += 1;
            }
        }
    }

    std::vector<double> radialMean(maxRadius, 0.0);
    for (int r = 0; r < maxRadius; ++r) {
        radialMean[r] = radialCount[r] > 0 ? radialSum[r] / radialCount[r] : 0.0;
    }

    // Natural images: radial mean magnitude decays roughly monotonically. Fit a simple
    // linear trend over radius and measure residual variance in the outer 30% band —
    // GAN/diffusion checkerboard artifacts show up as bumps that break the smooth decay.
    const int highFreqStart = static_cast<int>(maxRadius * 0.7);
    double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    const int n = maxRadius - 1; // skip r=0 (DC term dominates and isn't informative)
    for (int r = 1; r < maxRadius; ++r) {
        sumX += r; sumY += radialMean[r]; sumXY += r * radialMean[r]; sumXX += r * r;
    }
    const double slope = (n * sumXY - sumX * sumY) / std::max(1e-9, (n * sumXX - sumX * sumX));
    const double intercept = (sumY - slope * sumX) / std::max(1, n);

    double residualSqSum = 0.0;
    int residualCount = 0;
    for (int r = highFreqStart; r < maxRadius; ++r) {
        const double predicted = intercept + slope * r;
        const double residual = radialMean[r] - predicted;
        residualSqSum += residual * residual;
        residualCount++;
    }
    const double highFreqResidualStd = residualCount > 0 ? std::sqrt(residualSqSum / residualCount) : 0.0;

    // Empirically-reasoned normalization: natural JPEG photos typically show residual
    // std well under ~0.15 (log-magnitude units) in the high-frequency band; synthetic
    // upsampling artifacts tend to push this notably higher.
    const double score = std::clamp(highFreqResidualStd / 0.35, 0.0, 1.0);

    evidence.score = score;
    evidence.confidence = 0.5; // one heuristic signal among several; moderate standalone confidence
    evidence.explanation = score > 0.6
        ? "We found a faint, repeating grid-like pattern in this image's fine detail — the kind AI image "
          "generators often leave behind, and real camera photos usually don't have."
        : "The fine detail in this image looks smooth and natural, the way real camera photos usually look.";
    evidence.rawDetails["highFreqResidualStd"] = highFreqResidualStd;

    // Visualization: the log-magnitude spectrum itself, normalized to 0-255.
    cv::Mat normalized;
    cv::normalize(magnitude, normalized, 0, 255, cv::NORM_MINMAX);
    normalized.convertTo(normalized, CV_8U);
    cv::Mat colorized;
    cv::applyColorMap(normalized, colorized, cv::COLORMAP_VIRIDIS);
    std::vector<uint8_t> pngBytes;
    cv::imencode(".png", colorized, pngBytes);
    evidence.visualizationPngBase64 = base64Encode(pngBytes);

    return evidence;
}

} // namespace fakede
