#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Inspects EXIF/XMP metadata for signals that correlate with editing or synthesis:
// absence of camera make/model (real photos almost always carry these), presence of
// editor-software tags (Photoshop, GIMP, known AI tools), and timestamp inconsistencies.
// Weighted down in fusion relative to the ML/pixel-level analyzers since metadata is
// trivially stripped or forged — its absence is weak evidence, not proof.
class MetadataAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "metadata"; }
    std::string humanLabel() const override { return "Metadata Forensics"; }
    std::vector<std::string> supportedMimeTypes() const override;
    bool isAvailable() const override { return true; } // no external assets needed
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
