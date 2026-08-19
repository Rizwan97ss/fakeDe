#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

class ElaAnalyzer;
class FrequencyAnalyzer;
class NoiseResidualAnalyzer;

// Samples frames across the video and runs the classical image forensic analyzers
// (ELA, frequency-spectrum, noise-residual) on each one, then measures how consistent
// their scores are frame-to-frame. Continuous footage from a single camera/encode
// session should show fairly stable classical-forensic characteristics; a swapped
// face region or per-frame-regenerated content tends to show more inconsistency.
// Reuses the same analyzer instances the image pipeline uses (passed in by pointer,
// owned by AnalyzerRegistry) rather than duplicating their logic.
class VideoTemporalConsistencyAnalyzer : public IAnalyzer {
public:
    VideoTemporalConsistencyAnalyzer(const ElaAnalyzer* ela, const FrequencyAnalyzer* freq,
                                      const NoiseResidualAnalyzer* noise);

    std::string id() const override { return "video-temporal-consistency"; }
    std::string humanLabel() const override { return "Frame-to-Frame Consistency"; }
    std::vector<std::string> supportedMimeTypes() const override { return {"video/mp4"}; }
    bool isAvailable() const override { return true; }
    Evidence analyze(const AnalysisInput& input) const override;

private:
    const ElaAnalyzer* ela_;
    const FrequencyAnalyzer* freq_;
    const NoiseResidualAnalyzer* noise_;
};

} // namespace fakede
