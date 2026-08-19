#include "SpliceDetectionAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

#include "AudioFrameAnalysis.h"
#include "util/AudioDecoder.h"

namespace fakede {

namespace {
constexpr double kQuietPercentile = 0.2; // bottom 20% of frames by energy = "quiet"
constexpr size_t kMinQuietFrames = 15;
} // namespace

Evidence SpliceDetectionAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    const auto decoded = AudioDecoder::decode(input.bytes);
    if (!decoded) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "Audio could not be decoded.";
        return evidence;
    }

    const auto frames = analyzeFrames(decoded->monoSamples, decoded->sampleRate);
    if (frames.size() < kMinQuietFrames * 5) {
        evidence.score = 0.5;
        evidence.confidence = 0.05;
        evidence.explanation = "Audio is too short for reliable noise-floor analysis.";
        return evidence;
    }

    constexpr double kEps = 1e-9;
    std::vector<double> dbLevels;
    dbLevels.reserve(frames.size());
    for (const auto& f : frames) dbLevels.push_back(20.0 * std::log10(f.rms + kEps));

    std::vector<double> sorted = dbLevels;
    std::sort(sorted.begin(), sorted.end());
    const double threshold = sorted[static_cast<size_t>(sorted.size() * kQuietPercentile)];

    std::vector<double> quietLevels;
    for (double db : dbLevels) {
        if (db <= threshold) quietLevels.push_back(db);
    }

    if (quietLevels.size() < kMinQuietFrames) {
        evidence.score = 0.5;
        evidence.confidence = 0.1;
        evidence.explanation = "Not enough quiet segments found to establish a noise floor.";
        return evidence;
    }

    const double meanDb = std::accumulate(quietLevels.begin(), quietLevels.end(), 0.0) / quietLevels.size();
    double sqSum = 0.0;
    for (double db : quietLevels) sqSum += (db - meanDb) * (db - meanDb);
    const double stdDb = std::sqrt(sqSum / quietLevels.size());

    // Provisional, reasoned threshold - a single consistent recording session
    // typically shows quiet-segment noise floor variation well under a few dB.
    const double score = std::clamp(stdDb / 6.0, 0.0, 1.0);
    const double confidence = std::clamp(0.25 + 0.25 * (quietLevels.size() / 60.0), 0.0, 0.5);

    evidence.score = score;
    evidence.confidence = confidence;
    std::ostringstream explanation;
    explanation << "Background noise floor across quiet segments varies by " << std::round(stdDb * 10) / 10.0
                << " dB - " << (score > 0.5 ? "inconsistent enough to suggest audio from different sources or "
                                               "recording sessions was combined."
                                             : "consistent with a single, unedited recording.");
    evidence.explanation = explanation.str();
    evidence.rawDetails["noiseFloorStdDb"] = stdDb;
    evidence.rawDetails["quietFrameCount"] = quietLevels.size();

    return evidence;
}

} // namespace fakede
