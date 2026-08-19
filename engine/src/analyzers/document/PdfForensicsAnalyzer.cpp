#include "PdfForensicsAnalyzer.h"

#include <algorithm>
#include <sstream>
#include <string_view>

namespace fakede {

namespace {

size_t countOccurrences(std::string_view haystack, std::string_view needle) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

// Naive extraction of a PDF literal string value for a given dictionary key, e.g.
// "/Producer (Adobe Acrobat 24.1)" -> "Adobe Acrobat 24.1". Stops at the first
// unescaped ')' - doesn't handle nested parens or octal escapes, adequate for the
// common case of a short software-name string.
std::string extractLiteralStringValue(std::string_view haystack, std::string_view key) {
    const size_t keyPos = haystack.find(key);
    if (keyPos == std::string_view::npos) return "";
    const size_t openParen = haystack.find('(', keyPos);
    if (openParen == std::string_view::npos || openParen - keyPos > 10) return "";
    const size_t closeParen = haystack.find(')', openParen);
    if (closeParen == std::string_view::npos) return "";
    return std::string(haystack.substr(openParen + 1, closeParen - openParen - 1));
}

} // namespace

Evidence PdfForensicsAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    const std::string_view bytes(reinterpret_cast<const char*>(input.bytes.data()), input.bytes.size());

    const size_t eofCount = countOccurrences(bytes, "%%EOF");
    const size_t prevCount = countOccurrences(bytes, "/Prev");
    const bool hasSignature = bytes.find("/Type/Sig") != std::string_view::npos ||
                               bytes.find("/Type /Sig") != std::string_view::npos;

    const std::string producer = extractLiteralStringValue(bytes, "/Producer");
    const std::string creator = extractLiteralStringValue(bytes, "/Creator");

    // A single %%EOF and no /Prev is a freshly-generated, never-incrementally-updated
    // file. Each additional revision is real evidence of a later edit.
    const size_t extraRevisions = eofCount > 0 ? eofCount - 1 : 0;
    double score = std::clamp(extraRevisions * 0.3 + (prevCount > 0 ? 0.25 : 0.0), 0.0, 1.0);
    if (hasSignature && extraRevisions > 0) {
        // Edits after a digital signature was applied is a materially stronger signal
        // than an ordinary multi-revision file.
        score = std::clamp(score + 0.25, 0.0, 1.0);
    }

    evidence.score = score;
    // Moderate, not high: the underlying byte-scan technique is reliable for what it
    // does detect, but this analyzer misses revisions stored in compressed
    // cross-reference streams (PDF 1.5+), so absence of a signal here is weak evidence
    // of authenticity, not proof.
    evidence.confidence = 0.5;

    std::ostringstream explanation;
    if (extraRevisions == 0 && prevCount == 0) {
        explanation << "No incremental-update revisions detected - consistent with a file that hasn't been edited "
                       "since it was generated (or was edited via a compressed cross-reference stream, which this "
                       "check can't see).";
    } else {
        explanation << "Found " << (extraRevisions + 1) << " revision(s) in this PDF's update history";
        if (hasSignature) explanation << ", including at least one edit after a digital signature was present";
        explanation << ". This means the file was edited after its initial generation - not necessarily malicious "
                       "(e.g. a filled-out form), but worth knowing.";
    }
    evidence.explanation = explanation.str();

    evidence.rawDetails["eofCount"] = eofCount;
    evidence.rawDetails["prevKeyCount"] = prevCount;
    evidence.rawDetails["hasSignature"] = hasSignature;
    if (!producer.empty()) evidence.rawDetails["producer"] = producer;
    if (!creator.empty()) evidence.rawDetails["creator"] = creator;

    return evidence;
}

} // namespace fakede
