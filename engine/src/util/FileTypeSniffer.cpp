#include "FileTypeSniffer.h"

#include <magic.h>

namespace fakede {

FileTypeSniffer::FileTypeSniffer() {
    magic_t cookie = magic_open(MAGIC_MIME_TYPE);
    if (cookie && magic_load(cookie, nullptr) == 0) {
        magicCookie_ = cookie;
    } else {
        if (cookie) magic_close(cookie);
        magicCookie_ = nullptr;
    }
}

FileTypeSniffer::~FileTypeSniffer() {
    if (magicCookie_) {
        magic_close(static_cast<magic_t>(magicCookie_));
    }
}

std::string FileTypeSniffer::detectMimeType(const std::vector<uint8_t>& bytes) const {
    if (!magicCookie_ || bytes.empty()) {
        return "application/octet-stream";
    }
    const char* mime = magic_buffer(static_cast<magic_t>(magicCookie_), bytes.data(), bytes.size());
    if (!mime) {
        return "application/octet-stream";
    }
    return std::string(mime);
}

} // namespace fakede
