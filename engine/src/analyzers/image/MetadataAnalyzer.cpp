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
            explanation = "This picture has no hidden camera details attached (like what phone or camera took "
                          "it). That's normal for AI-made images, but also very common for ordinary photos "
                          "shared online, so this clue alone doesn't tell us much.";
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
                explanation = "This file's hidden details name a known AI image tool — about as clear as this "
                              "kind of evidence gets.";
            } else if (hasMake && hasModel) {
                score = 0.15;
                confidence = 0.6;
                explanation = "This file has real camera details attached (make and model), the kind a genuine "
                              "photo usually carries.";
            } else if (editorTagFound) {
                score = 0.6;
                confidence = 0.4;
                explanation = "This file's hidden details show it was opened in regular photo-editing software "
                              "— it's been edited by someone at some point, though not necessarily by AI.";
            } else {
                score = 0.5;
                confidence = 0.25;
                explanation = "This file has some hidden details attached, but nothing pointing clearly to a "
                              "camera or to AI software — doesn't tell us much either way.";
            }
        }
    } catch (const Exiv2::Error&) {
        score = 0.5;
        confidence = 0.15;
        explanation = "We couldn't read this file's hidden details.";
    }

    evidence.score = score;
    evidence.confidence = confidence;
    evidence.explanation = explanation;
    return evidence;
}

} // namespace fakede
