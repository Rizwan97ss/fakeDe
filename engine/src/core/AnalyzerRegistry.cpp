#include "AnalyzerRegistry.h"

#include <algorithm>
#include <filesystem>

#include "analyzers/image/AiGeneratedModelAnalyzer.h"
#include "analyzers/image/ElaAnalyzer.h"
#include "analyzers/image/FrequencyAnalyzer.h"
#include "analyzers/image/MetadataAnalyzer.h"
#include "analyzers/image/NoiseResidualAnalyzer.h"

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

    return registry;
}

} // namespace fakede
