#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Incremental-update forensics for PDFs: counts "%%EOF" markers and "/Prev" trailer
// keys via direct byte scanning of the raw file. The PDF spec's incremental-update
// mechanism appends a new revision (its own xref/trailer/%%EOF) rather than rewriting
// the file on every edit, so more than one of either is a genuine, well-documented
// tampering/editing signal - not a heuristic guess.
//
// Deliberately NOT a full PDF parser: this misses content inside compressed
// cross-reference/object streams (PDF 1.5+, common in modern PDF writers), so it
// under-counts on some files rather than over-claiming. Says "edited", not
// "tampered" - a form filled out digitally also produces incremental updates and
// isn't inherently malicious.
class PdfForensicsAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "pdf-forensics"; }
    std::string humanLabel() const override { return "PDF Revision Forensics"; }
    std::vector<std::string> supportedMimeTypes() const override { return {"application/pdf"}; }
    bool isAvailable() const override { return true; }
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
