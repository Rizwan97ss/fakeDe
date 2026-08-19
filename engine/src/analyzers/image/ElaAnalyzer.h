#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Error Level Analysis: recompress the image at a known JPEG quality and diff against
// the original. Regions that were spliced in or heavily edited after the last save
// compress differently than the rest of the image and light up in the diff. Produces a
// heatmap visualization for the evidence UI. A classic, well-understood forensic
// technique — not ML-based, complements the pixel-statistics-driven ML analyzer.
class ElaAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "ela"; }
    std::string humanLabel() const override { return "Error Level Analysis"; }
    std::vector<std::string> supportedMimeTypes() const override;
    bool isAvailable() const override { return true; }
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
