#pragma once

#include "Types.h"

namespace fakede {

// Extension point for every file type. Adding support for a new media type or a new
// detection technique means writing one of these and registering it with
// AnalyzerRegistry — nothing else in the pipeline needs to change.
class IAnalyzer {
public:
    virtual ~IAnalyzer() = default;

    // Stable machine-readable id, used as Evidence::analyzerId (e.g. "ela", "freq-fft").
    virtual std::string id() const = 0;

    virtual std::string humanLabel() const = 0;

    // MIME types this analyzer knows how to handle, e.g. {"image/jpeg", "image/png"}.
    virtual std::vector<std::string> supportedMimeTypes() const = 0;

    // False when required assets (e.g. an ONNX model file) failed to load. The pipeline
    // skips unavailable analyzers rather than failing the whole request, so the product
    // degrades gracefully instead of going down when one model is missing.
    virtual bool isAvailable() const = 0;

    virtual Evidence analyze(const AnalysisInput& input) const = 0;
};

} // namespace fakede
