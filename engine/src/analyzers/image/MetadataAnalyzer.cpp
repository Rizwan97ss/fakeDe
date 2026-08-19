#include "MetadataAnalyzer.h"

#include <exiv2/exiv2.hpp>

#include <algorithm>
#include <array>
#include <cctype>

namespace fakede {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Software tag substrings known to be written by generative-AI tools. Checked
// case-insensitively against Exif.Image.Software / XMP creator-tool fields.
constexpr std::array<const char*, 8> kKnownAiSoftwareTags = {
    "midjourney", "dall-e", "dall·e", "stable diffusion", "stability.ai",
    "firefly", "dreamstudio", "runwayml",
};

// Conventional (non-AI) editors: presence indicates the image was edited, which is a
// much weaker signal than an explicit AI-tool tag.
constexpr std::array<const char*, 4> kKnownEditorTags = {
    "photoshop", "gimp", "lightroom", "affinity photo",
};

} // namespace

std::vector<std::string> MetadataAnalyzer::supportedMimeTypes() const {
    return {"image/jpeg", "image/png", "image/tiff", "image/webp"};
}

Evidence MetadataAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    double score = 0.5;
    double confidence = 0.35; // metadata is easy to strip/forge; never highly confident on its own
    std::string explanation;

    try {
        auto image = Exiv2::ImageFactory::open(input.bytes.data(), input.bytes.size());
        image->readMetadata();
        const Exiv2::ExifData& exif = image->exifData();

        if (exif.empty()) {
            score = 0.58;
            confidence = 0.3;
            explanation = "No EXIF metadata present. Common for AI-generated images and for web-downloaded or "
                          "screenshot images alike, so this alone is weak evidence.";
        } else {
            const bool hasMake = exif.findKey(Exiv2::ExifKey("Exif.Image.Make")) != exif.end();
            const bool hasModel = exif.findKey(Exiv2::ExifKey("Exif.Image.Model")) != exif.end();

            std::string software;
            auto swIt = exif.findKey(Exiv2::ExifKey("Exif.Image.Software"));
            if (swIt != exif.end()) software = toLower(swIt->toString());

            const bool aiTagFound = std::any_of(kKnownAiSoftwareTags.begin(), kKnownAiSoftwareTags.end(),
                                                 [&](const char* tag) { return software.find(tag) != std::string::npos; });
            const bool editorTagFound = std::any_of(kKnownEditorTags.begin(), kKnownEditorTags.end(),
                                                      [&](const char* tag) { return software.find(tag) != std::string::npos; });

            if (aiTagFound) {
                score = 0.95;
                confidence = 0.9;
                explanation = "Software tag identifies a known generative-AI tool.";
            } else if (hasMake && hasModel) {
                score = 0.15;
                confidence = 0.6;
                explanation = "Camera make/model metadata present, consistent with a real camera capture.";
            } else if (editorTagFound) {
                score = 0.6;
                confidence = 0.4;
                explanation = "Software tag indicates conventional photo-editing software was used.";
            } else {
                score = 0.5;
                confidence = 0.25;
                explanation = "EXIF metadata present but inconclusive: no camera or known editor/AI software identified.";
            }
        }
    } catch (const Exiv2::Error&) {
        score = 0.5;
        confidence = 0.15;
        explanation = "Metadata could not be parsed for this file.";
    }

    evidence.score = score;
    evidence.confidence = confidence;
    evidence.explanation = explanation;
    return evidence;
}

} // namespace fakede
