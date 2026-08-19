#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fakede {

// Wraps libmagic to identify a file's real MIME type from its bytes. We never trust a
// client-supplied filename extension or Content-Type header for routing analysis —
// that's trivial to spoof and would silently misroute e.g. a renamed .exe through the
// image pipeline.
class FileTypeSniffer {
public:
    FileTypeSniffer();
    ~FileTypeSniffer();

    FileTypeSniffer(const FileTypeSniffer&) = delete;
    FileTypeSniffer& operator=(const FileTypeSniffer&) = delete;

    // Returns e.g. "image/jpeg". Returns "application/octet-stream" if libmagic can't
    // determine a type or failed to initialize.
    std::string detectMimeType(const std::vector<uint8_t>& bytes) const;

private:
    void* magicCookie_ = nullptr; // magic_t, kept opaque to avoid leaking <magic.h> here
};

} // namespace fakede
