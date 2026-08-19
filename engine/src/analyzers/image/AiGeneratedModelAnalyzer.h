#pragma once

#include <memory>
#include <string>

#include "core/IAnalyzer.h"
#include "onnx/OnnxSession.h"

namespace fakede {

// Runs a pretrained AI-generated-image classifier (ONNX export of e.g.
// WisconsinAIVision/UniversalFakeDetect or chuangchuangtan/NPR-DeepfakeDetection —
// see docs/model-sourcing.md for the exact model, license, and conversion notes).
// This is the highest-weighted analyzer in fusion since it's a purpose-built
// classifier rather than a generic forensic heuristic, but it only runs at all when
// the .onnx weight file has actually been fetched (see scripts/fetch_models.ps1) —
// isAvailable() reports false otherwise so the rest of the pipeline still works.
class AiGeneratedModelAnalyzer : public IAnalyzer {
public:
    explicit AiGeneratedModelAnalyzer(const std::string& modelPath);

    std::string id() const override { return "ai-model:image-classifier"; }
    std::string humanLabel() const override { return "AI-Generated Image Classifier"; }
    std::vector<std::string> supportedMimeTypes() const override;
    bool isAvailable() const override { return session_->isLoaded(); }
    Evidence analyze(const AnalysisInput& input) const override;

private:
    std::unique_ptr<OnnxSession> session_;
};

} // namespace fakede
