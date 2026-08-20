#include "FileTypeSniffer.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <set>
#include <string_view>

namespace fakede {

namespace {

bool startsWith(const std::vector<uint8_t>& bytes, std::initializer_list<uint8_t> sig) {
    if (bytes.size() < sig.size()) return false;
    return std::equal(sig.begin(), sig.end(), bytes.begin());
}

// ISO base media file format (ISOBMFF) - MP4, HEIC/HEIF, and AVIF all share this exact
// container (a 4-byte box size, "ftyp", then a 4-byte major-brand fourCC at offset 8)
// and are only distinguishable by that brand. Without this, HEIC/AVIF files were
// silently mislabeled as video/mp4 - a real bug fixed alongside this format expansion.
std::string mimeTypeForIsobmffBrand(const std::vector<uint8_t>& bytes) {
    const std::string brand(reinterpret_cast<const char*>(bytes.data() + 8), 4);
    static const std::set<std::string> kHeicBrands = {"heic", "heix", "heim", "heis",
                                                        "hevc", "hevx", "hevm", "hevs", "mif1", "msf1"};
    static const std::set<std::string> kAvifBrands = {"avif", "avis"};
    if (kHeicBrands.count(brand)) return "image/heic";
    if (kAvifBrands.count(brand)) return "image/avif";
    return "video/mp4";
}

// ZIP-based document formats share the plain ZIP signature and are only
// distinguishable by their internal contents. ZIP stores entry filenames as literal,
// uncompressed ASCII in each local file header, so a raw substring scan reliably finds
// these markers without needing to actually decompress anything (no zip-bomb risk -
// this never inflates a single byte of archive content).
std::string mimeTypeForZipContents(const std::vector<uint8_t>& bytes) {
    const std::string_view content(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    auto has = [&](std::string_view marker) { return content.find(marker) != std::string_view::npos; };

    if (has("word/document.xml")) {
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    }
    if (has("xl/workbook.xml")) {
        return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    }
    if (has("ppt/presentation.xml")) {
        return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    }
    // ODF formats store an uncompressed "mimetype" entry, literally containing the
    // MIME string, as the archive's first entry.
    if (has("mimetypeapplication/vnd.oasis.opendocument.text")) return "application/vnd.oasis.opendocument.text";
    if (has("mimetypeapplication/vnd.oasis.opendocument.spreadsheet")) {
        return "application/vnd.oasis.opendocument.spreadsheet";
    }
    if (has("mimetypeapplication/vnd.oasis.opendocument.presentation")) {
        return "application/vnd.oasis.opendocument.presentation";
    }
    return "application/zip";
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
    // ISO base media file format: a 4-byte box size, then "ftyp", then a major-brand
    // fourCC that distinguishes MP4 from HEIC/HEIF and AVIF (see helper above).
    if (bytes.size() >= 12 && std::memcmp(bytes.data() + 4, "ftyp", 4) == 0) {
        return mimeTypeForIsobmffBrand(bytes);
    }
    if (startsWith(bytes, {0x47, 0x49, 0x46, 0x38})) { // "GIF8" (87a or 89a)
        return "image/gif";
    }
    if (startsWith(bytes, {0x00, 0x00, 0x01, 0x00})) {
        return "image/x-icon";
    }
    if (startsWith(bytes, {0x1F, 0x8B})) {
        return "application/gzip";
    }
    if (startsWith(bytes, {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C})) {
        return "application/x-7z-compressed";
    }
    if (startsWith(bytes, {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07})) { // "Rar!\x1A\x07" (v1.5-4 and v5)
        return "application/vnd.rar";
    }
    // ZIP local-file-header ("PK\x03\x04") - checked last among archive signatures
    // since Office/ODF documents are ZIP containers distinguished only by contents.
    if (startsWith(bytes, {0x50, 0x4B, 0x03, 0x04})) {
        return mimeTypeForZipContents(bytes);
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
