#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Classical stylometric signals that need no model: sentence-length burstiness,
// vocabulary richness, and repeated-phrase rate. No single one of these reliably
// separates human from AI text on its own (see docs/model-sourcing.md - text is the
// least reliable detection domain in this whole product), so this analyzer
// deliberately caps its own confidence low regardless of how extreme its inputs are.
class TextStylometryAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "text-stylometry"; }
    std::string humanLabel() const override { return "Stylometric Analysis"; }
    std::vector<std::string> supportedMimeTypes() const override { return {"text/plain"}; }
    bool isAvailable() const override { return true; }
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
