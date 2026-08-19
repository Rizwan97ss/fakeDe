#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Estimates sensor-noise residual (via a fast Laplacian-based noise estimator) both
// globally and per-block. Real camera sensors leave a faint, fairly uniform noise floor
// across an entire photo; fully AI-generated images tend to look unnaturally smooth
// (very low noise), while spliced composites show patchy, inconsistent noise levels
// between the pasted region and its surroundings.
class NoiseResidualAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "noise-residual"; }
    std::string humanLabel() const override { return "Sensor Noise Residual"; }
    std::vector<std::string> supportedMimeTypes() const override;
    bool isAvailable() const override { return true; }
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
