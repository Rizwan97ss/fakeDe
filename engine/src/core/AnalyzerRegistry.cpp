#include "AnalyzerRegistry.h"

#include <algorithm>
#include <filesystem>

#include "analyzers/audio/AntiSpoofingAnalyzer.h"
#include "analyzers/audio/SpliceDetectionAnalyzer.h"
#include "analyzers/audio/VoiceNaturalnessAnalyzer.h"
#include "analyzers/document/PdfForensicsAnalyzer.h"
#include "analyzers/image/AiGeneratedModelAnalyzer.h"
#include "analyzers/image/ElaAnalyzer.h"
#include "analyzers/image/FrequencyAnalyzer.h"
#include "analyzers/image/MetadataAnalyzer.h"
#include "analyzers/image/NoiseResidualAnalyzer.h"
#include "analyzers/text/TextPerplexityAnalyzer.h"
#include "analyzers/text/TextStylometryAnalyzer.h"

namespace fakede {

void AnalyzerRegistry::registerAnalyzer(std::unique_ptr<IAnalyzer> analyzer) {
    analyzers_.push_back(std::move(analyzer));
}

std::vector<const IAnalyzer*> AnalyzerRegistry::analyzersFor(const std::string& mimeType) const {
    std::vector<const IAnalyzer*> result;
    for (const auto& analyzer : analyzers_) {
        if (!analyzer->isAvailable()) continue;
        const auto& supported = analyzer->supportedMimeTypes();
        if (std::find(supported.begin(), supported.end(), mimeType) != supported.end()) {
            result.push_back(analyzer.get());
        }
    }
    return result;
}

AnalyzerRegistry buildDefaultRegistry(const std::string& modelsDir) {
    AnalyzerRegistry registry;

    // Classical forensic analyzers: no external assets, always available.
    registry.registerAnalyzer(std::make_unique<MetadataAnalyzer>());
    registry.registerAnalyzer(std::make_unique<ElaAnalyzer>());
    registry.registerAnalyzer(std::make_unique<FrequencyAnalyzer>());
    registry.registerAnalyzer(std::make_unique<NoiseResidualAnalyzer>());

    // ML analyzer: gracefully unavailable until scripts/fetch_models.ps1 has been run.
    const std::filesystem::path imageModelPath =
        std::filesystem::path(modelsDir) / "image" / "ai_image_classifier.onnx";
    registry.registerAnalyzer(std::make_unique<AiGeneratedModelAnalyzer>(imageModelPath.string()));

    // Phase 2: text and documents.
    registry.registerAnalyzer(std::make_unique<TextStylometryAnalyzer>());
    registry.registerAnalyzer(std::make_unique<PdfForensicsAnalyzer>());

    const std::filesystem::path textModelsDir = std::filesystem::path(modelsDir) / "text";
    registry.registerAnalyzer(std::make_unique<TextPerplexityAnalyzer>(
        (textModelsDir / "gpt2.onnx").string(), (textModelsDir / "vocab.json").string(),
        (textModelsDir / "merges.txt").string()));

    // Phase 3: audio.
    registry.registerAnalyzer(std::make_unique<VoiceNaturalnessAnalyzer>());
    registry.registerAnalyzer(std::make_unique<SpliceDetectionAnalyzer>());

    const std::filesystem::path audioModelPath =
        std::filesystem::path(modelsDir) / "audio" / "antispoofing.onnx";
    registry.registerAnalyzer(std::make_unique<AntiSpoofingAnalyzer>(audioModelPath.string()));

    return registry;
}

} // namespace fakede
