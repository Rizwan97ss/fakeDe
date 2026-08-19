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
// This deliberately checks a handful of well-known signatures directly rather than
// depending on libmagic's full type database: Phase 1 only ever routes to image
// analyzers, so recognizing image formats is all that's needed, and it avoids a heavy
// autotools dependency for a handful of fixed byte patterns. Revisit if a future phase
// (documents, audio, video) needs broader format coverage.
class FileTypeSniffer {
public:
    // Returns e.g. "image/jpeg". Returns "application/octet-stream" if no known
    // signature matches.
    std::string detectMimeType(const std::vector<uint8_t>& bytes) const;
};

} // namespace fakede
