#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Looks for an embedded C2PA "Content Credentials" manifest (the provenance standard
// OpenAI, Adobe, and others now stamp into generated/captured images) via a raw
// byte/string scan for known JUMBF/C2PA markers - not a full JUMBF/CBOR/COSE parser,
// and deliberately not a cryptographic signature verification. When present and
// explicit, this is unusually strong evidence (the file is self-declaring its own
// origin); when absent, that says almost nothing, since most images never carry this
// and it's trivially stripped by re-saving/re-encoding/re-hosting.
class C2paManifestAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "c2pa-manifest"; }
    std::string humanLabel() const override { return "AI Content Credentials (C2PA)"; }
    std::vector<std::string> supportedMimeTypes() const override;
    bool isAvailable() const override { return true; } // no external assets needed
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
