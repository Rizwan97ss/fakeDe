#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Frequency-domain (2D FFT) analysis. GAN/diffusion decoders that upsample via
// transposed convolutions or pixel-shuffle layers tend to leave periodic, grid-like
// artifacts that show up as sharp peaks in the high-frequency band of the power
// spectrum — real camera photos have a much smoother, roughly power-law falloff.
class FrequencyAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "freq-fft"; }
    std::string humanLabel() const override { return "Frequency Spectrum Analysis"; }
    std::vector<std::string> supportedMimeTypes() const override;
    bool isAvailable() const override { return true; }
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
