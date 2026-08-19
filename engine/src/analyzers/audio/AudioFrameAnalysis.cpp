#include "AudioFrameAnalysis.h"

#include <algorithm>
#include <cmath>

namespace fakede {

namespace {
constexpr double kFrameSeconds = 0.04;    // 40ms
constexpr double kHopSeconds = 0.02;      // 20ms (50% overlap)
constexpr double kMinPitchHz = 70.0;      // low end of typical human voice
constexpr double kMaxPitchHz = 400.0;     // high end
constexpr double kVoicingThreshold = 0.3; // min normalized autocorrelation to call a frame "voiced"
} // namespace

std::vector<AudioFrameStats> analyzeFrames(const std::vector<float>& samples, uint32_t sampleRate) {
    std::vector<AudioFrameStats> frames;
    if (sampleRate == 0) return frames;

    const int frameSize = static_cast<int>(sampleRate * kFrameSeconds);
    const int hopSize = static_cast<int>(sampleRate * kHopSeconds);
    const int minLag = std::max(1, static_cast<int>(sampleRate / kMaxPitchHz));
    const int maxLag = static_cast<int>(sampleRate / kMinPitchHz);
    if (frameSize <= 0 || hopSize <= 0 || maxLag <= minLag) return frames;

    for (int start = 0; start + frameSize <= static_cast<int>(samples.size()); start += hopSize) {
        double sumSq = 0.0;
        for (int i = 0; i < frameSize; ++i) {
            const double v = samples[start + i];
            sumSq += v * v;
        }
        AudioFrameStats stats;
        stats.rms = std::sqrt(sumSq / frameSize);

        double bestNormCorr = 0.0;
        int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; ++lag) {
            const int n = frameSize - lag;
            if (n <= 0) break;
            double corr = 0.0, energyA = 0.0, energyB = 0.0;
            for (int i = 0; i < n; ++i) {
                const double a = samples[start + i];
                const double b = samples[start + i + lag];
                corr += a * b;
                energyA += a * a;
                energyB += b * b;
            }
            const double denom = std::sqrt(energyA * energyB);
            const double normCorr = denom > 1e-9 ? corr / denom : 0.0;
            if (normCorr > bestNormCorr) {
                bestNormCorr = normCorr;
                bestLag = lag;
            }
        }

        stats.period = (bestNormCorr > kVoicingThreshold) ? static_cast<double>(bestLag) : 0.0;
        frames.push_back(stats);
    }

    return frames;
}

} // namespace fakede
