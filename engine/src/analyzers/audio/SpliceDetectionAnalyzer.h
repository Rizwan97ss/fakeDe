#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Finds the quietest ~20% of frames (a robust proxy for background/room noise floor
// without needing real voice-activity detection) and checks how consistent their
// noise-floor level is across the file. A recording spliced together from different
// sources typically has a different background-noise level in each source's quiet
// segments; a single, unedited recording has one consistent noise floor throughout.
class SpliceDetectionAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "audio-splice-detection"; }
    std::string humanLabel() const override { return "Noise Floor Consistency"; }
    std::vector<std::string> supportedMimeTypes() const override { return {"audio/wav"}; }
    bool isAvailable() const override { return true; }
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
