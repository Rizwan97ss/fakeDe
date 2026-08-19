#include "Types.h"

namespace fakede {

nlohmann::json Evidence::toJson() const {
    nlohmann::json j{
        {"analyzerId", analyzerId},
        {"humanLabel", humanLabel},
        {"score", score},
        {"confidence", confidence},
        {"explanation", explanation},
        {"rawDetails", rawDetails},
    };
    if (visualizationPngBase64) {
        j["visualizationPngBase64"] = *visualizationPngBase64;
    } else {
        j["visualizationPngBase64"] = nullptr;
    }
    return j;
}

std::string toString(VerdictLabel label) {
    switch (label) {
        case VerdictLabel::LikelyAuthentic:
            return "likely_authentic";
        case VerdictLabel::Inconclusive:
            return "inconclusive";
        case VerdictLabel::LikelyAiGeneratedOrAltered:
            return "likely_ai_generated_or_altered";
    }
    return "inconclusive";
}

nlohmann::json Verdict::toJson() const {
    nlohmann::json evidenceJson = nlohmann::json::array();
    for (const auto& e : evidenceBreakdown) {
        evidenceJson.push_back(e.toJson());
    }
    return nlohmann::json{
        {"overallLabel", toString(overallLabel)},
        {"overallScore", overallScore},
        {"overallConfidence", overallConfidence},
        {"evidenceBreakdown", evidenceJson},
    };
}

} // namespace fakede
