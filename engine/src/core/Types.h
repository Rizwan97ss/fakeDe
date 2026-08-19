#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace fakede {

// Raw bytes of an uploaded file plus whatever the caller already knows about it.
// Analyzers read from here; nothing here is mutated during analysis.
struct AnalysisInput {
    std::string fileName;
    std::string mimeType;      // as sniffed by FileTypeSniffer, not trusted from the client
    std::vector<uint8_t> bytes;
};

// One analyzer's independent opinion. `score` and `confidence` are deliberately
// separate axes: score is "how fake does this look", confidence is "how much should
// this particular signal be trusted for this particular input" (e.g. an ELA analyzer
// has low confidence on a heavily-downscaled image regardless of what score it computes).
struct Evidence {
    std::string analyzerId;                        // "ela", "freq-fft", "ai-model:universalfakedetect"
    std::string humanLabel;                         // "Error Level Analysis"
    double score = 0.0;                             // 0.0 authentic .. 1.0 fake/altered
    double confidence = 0.0;                        // 0.0 .. 1.0, this analyzer's own certainty
    std::string explanation;                        // shown verbatim in the evidence breakdown UI
    std::optional<std::string> visualizationPngBase64;  // e.g. ELA heatmap overlay, PNG bytes as base64
    nlohmann::json rawDetails = nlohmann::json::object();

    nlohmann::json toJson() const;
};

enum class VerdictLabel {
    LikelyAuthentic,
    Inconclusive,
    LikelyAiGeneratedOrAltered,
};

std::string toString(VerdictLabel label);

// The fused, top-level result returned by the API. Never a bare boolean —
// the evidence breakdown is what makes the verdict inspectable and honest.
struct Verdict {
    VerdictLabel overallLabel = VerdictLabel::Inconclusive;
    double overallScore = 0.0;         // fused 0.0 .. 1.0
    double overallConfidence = 0.0;    // fused 0.0 .. 1.0
    std::vector<Evidence> evidenceBreakdown;

    nlohmann::json toJson() const;
};

} // namespace fakede
