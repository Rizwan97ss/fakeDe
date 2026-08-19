#pragma once

#include <cstdint>
#include <vector>

namespace fakede {

struct AudioFrameStats {
    double rms = 0.0;      // linear RMS amplitude for this frame
    double period = 0.0;   // pitch period in samples; 0.0 means unvoiced/no clear pitch
};

// Frame-level pitch (via normalized autocorrelation) and RMS amplitude, used by both
// the jitter/shimmer and splice-detection analyzers. This is a per-frame
// approximation of the classical per-glottal-cycle jitter/shimmer measures used in
// clinical voice analysis - adequate for a comparative forensic signal, not a
// clinical-grade pitch tracker.
std::vector<AudioFrameStats> analyzeFrames(const std::vector<float>& samples, uint32_t sampleRate);

} // namespace fakede
