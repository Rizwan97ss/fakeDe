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
    // WebP and WAV share RIFF's container signature ("RIFF" + 4-byte size + a
    // 4-byte format tag), so both checks look at the same offset.
    if (bytes.size() >= 12 && std::memcmp(bytes.data(), "RIFF", 4) == 0) {
        if (std::memcmp(bytes.data() + 8, "WEBP", 4) == 0) return "image/webp";
        if (std::memcmp(bytes.data() + 8, "WAVE", 4) == 0) return "audio/wav";
    }
    if (bytes.size() >= 5 && std::memcmp(bytes.data(), "%PDF-", 5) == 0) {
        return "application/pdf";
    }
    // MP4/MOV (ISO base media file format): a 4-byte box size, then "ftyp".
    if (bytes.size() >= 8 && std::memcmp(bytes.data() + 4, "ftyp", 4) == 0) {
        return "video/mp4";
    }
    if (looksLikePlainText(bytes)) {
        return "text/plain";
    }
    return "application/octet-stream";
}

bool FileTypeSniffer::looksLikePlainText(const std::vector<uint8_t>& bytes) const {
    if (bytes.empty()) return false;
    // Sample the first 8KB: printable ASCII, common whitespace, or valid UTF-8
    // continuation bytes. A binary file (renamed .exe, etc.) fails this near-instantly
    // since NUL bytes and most control characters are disqualifying.
    const size_t sampleSize = std::min<size_t>(bytes.size(), 8192);
    size_t suspicious = 0;
    for (size_t i = 0; i < sampleSize; ++i) {
        const uint8_t b = bytes[i];
        const bool printableAscii = b == '\t' || b == '\n' || b == '\r' || (b >= 0x20 && b < 0x7F);
        const bool utf8Continuation = b >= 0x80; // not validated byte-by-byte, just not an outright control byte
        if (!printableAscii && !utf8Continuation) {
            ++suspicious;
        }
        if (b == 0x00) return false; // NUL is an unambiguous binary-file signal
    }
    // Allow a small margin for the occasional stray byte rather than requiring perfection.
    return suspicious == 0 || (static_cast<double>(suspicious) / sampleSize) < 0.01;
}

} // namespace fakede
