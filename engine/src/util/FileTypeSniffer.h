#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fakede {

// Identifies a file's real MIME type from its magic-byte signature. We never trust a
// client-supplied filename extension or Content-Type header for routing analysis —
// that's trivial to spoof and would silently misroute e.g. a renamed .exe through the
// image pipeline.
//
// This deliberately checks well-known signatures directly rather than depending on
// libmagic's full type database, avoiding a heavy autotools dependency (see
// docs/ARCHITECTURE.md for why libmagic was dropped entirely). Detecting a type here
// does not imply an analyzer exists for it yet: AnalyzerRegistry::analyzersFor()
// legitimately returns empty for a correctly-identified but not-yet-supported type
// (e.g. DOCX, gzip), and AnalysisController reports that honestly rather than
// guessing or silently misrouting the file through an unrelated analyzer.
class FileTypeSniffer {
public:
    // Returns e.g. "image/jpeg". Returns "application/octet-stream" if no known
    // signature matches.
    std::string detectMimeType(const std::vector<uint8_t>& bytes) const;

private:
    bool looksLikePlainText(const std::vector<uint8_t>& bytes) const;
};

} // namespace fakede
