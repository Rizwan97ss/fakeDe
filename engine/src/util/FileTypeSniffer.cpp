#include "FileTypeSniffer.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>

namespace fakede {

namespace {

bool startsWith(const std::vector<uint8_t>& bytes, std::initializer_list<uint8_t> sig) {
    if (bytes.size() < sig.size()) return false;
    return std::equal(sig.begin(), sig.end(), bytes.begin());
}

} // namespace

std::string FileTypeSniffer::detectMimeType(const std::vector<uint8_t>& bytes) const {
    if (startsWith(bytes, {0xFF, 0xD8, 0xFF})) {
        return "image/jpeg";
    }
    if (startsWith(bytes, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A})) {
        return "image/png";
    }
    if (startsWith(bytes, {0x42, 0x4D})) {
        return "image/bmp";
    }
    if (startsWith(bytes, {0x49, 0x49, 0x2A, 0x00}) || startsWith(bytes, {0x4D, 0x4D, 0x00, 0x2A})) {
        return "image/tiff";
    }
    // WebP: "RIFF" + 4-byte size + "WEBP", so the signature isn't contiguous.
    if (bytes.size() >= 12 && std::memcmp(bytes.data(), "RIFF", 4) == 0 &&
        std::memcmp(bytes.data() + 8, "WEBP", 4) == 0) {
        return "image/webp";
    }
    return "application/octet-stream";
}

} // namespace fakede
