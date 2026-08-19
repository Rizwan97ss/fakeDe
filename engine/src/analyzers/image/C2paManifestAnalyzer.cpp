#include "C2paManifestAnalyzer.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace fakede {

namespace {

bool containsBytes(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

// C2PA manifests are JUMBF boxes with CBOR-encoded assertions (not JSON), but CBOR
// text strings still carry their raw UTF-8 bytes with no special escaping, so a plain
// substring scan finds these markers without needing a real JUMBF/CBOR parser. This is
// deliberately the same lightweight-byte-scan tradeoff as PdfForensicsAnalyzer: we
// detect presence of known markers, we don't verify the manifest's cryptographic
// signature (COSE) or fully parse its structure.
constexpr std::array<const char*, 5> kC2paPresenceMarkers = {
    "c2pa.assertions", "c2pa.claim", "urn:c2pa", "claim_generator", "c2pa.signature",
};

// IPTC digitalSourceType values C2PA assertions use to declare origin.
constexpr std::array<const char*, 2> kAiSourceMarkers = {
    "trainedAlgorithmicMedia",
    "compositeWithTrainedAlgorithmicMedia",
};
constexpr const char* kRealCaptureMarker = "digitalCapture";

constexpr std::array<const char*, 9> kKnownGeneratorNames = {
    "OpenAI",  "DALL-E",       "ChatGPT",  "Midjourney",       "Firefly",
    "Stability AI", "Bing Image Creator", "Google AI", "Gemini",
};

std::string findMentionedGenerator(std::string_view haystack) {
    for (const char* name : kKnownGeneratorNames) {
        if (containsBytes(haystack, name)) return name;
    }
    return "";
}

} // namespace

std::vector<std::string> C2paManifestAnalyzer::supportedMimeTypes() const {
    return {"image/jpeg", "image/png", "image/webp"};
}

Evidence C2paManifestAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    const std::string_view bytes(reinterpret_cast<const char*>(input.bytes.data()), input.bytes.size());

    int presenceHits = 0;
    for (const char* marker : kC2paPresenceMarkers) {
        if (containsBytes(bytes, marker)) ++presenceHits;
    }

    const bool hasAiSourceMarker = std::any_of(kAiSourceMarkers.begin(), kAiSourceMarkers.end(),
                                                [&](const char* m) { return containsBytes(bytes, m); });
    const bool hasRealCaptureMarker = presenceHits > 0 && containsBytes(bytes, kRealCaptureMarker);
    const std::string generatorName = presenceHits > 0 ? findMentionedGenerator(bytes) : std::string();

    if (presenceHits > 0 && (hasAiSourceMarker || !generatorName.empty())) {
        evidence.score = 0.97;
        evidence.confidence = 0.9;
        evidence.explanation =
            "This file carries an embedded Content Credentials (C2PA) manifest that declares it as "
            "AI-generated" +
            (generatorName.empty() ? std::string(".") : (" (mentions " + generatorName + ").")) +
            " This is the file stating its own origin directly, not a guess - about as strong as evidence "
            "gets, though we checked for the marker text rather than cryptographically verifying the "
            "signature.";
    } else if (hasRealCaptureMarker) {
        evidence.score = 0.1;
        evidence.confidence = 0.6;
        evidence.explanation = "This file carries an embedded Content Credentials (C2PA) manifest that "
                               "declares it as a real camera capture, not AI-generated.";
    } else if (presenceHits >= 2) {
        evidence.score = 0.5;
        evidence.confidence = 0.2;
        evidence.explanation = "This file appears to carry some kind of Content Credentials (C2PA) "
                               "manifest, but we couldn't tell what it claims about the file's origin.";
    } else {
        evidence.score = 0.5;
        evidence.confidence = 0.05;
        evidence.explanation = "No Content Credentials (C2PA) manifest found. That's normal - most "
                               "images, real or AI-made, don't carry one, and it's easily stripped by "
                               "re-saving, converting, or re-uploading a file - so this alone says almost "
                               "nothing.";
    }

    evidence.rawDetails["c2paPresenceMarkersFound"] = presenceHits;
    evidence.rawDetails["aiSourceMarkerFound"] = hasAiSourceMarker;
    evidence.rawDetails["realCaptureMarkerFound"] = hasRealCaptureMarker;
    if (!generatorName.empty()) evidence.rawDetails["mentionedGenerator"] = generatorName;

    return evidence;
}

} // namespace fakede
