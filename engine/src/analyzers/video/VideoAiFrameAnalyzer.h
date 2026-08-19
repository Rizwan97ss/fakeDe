#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

class AiGeneratedModelAnalyzer;

// Samples frames and runs the same pretrained AI-image classifier used for still
// images (AiGeneratedModelAnalyzer - see docs/model-sourcing.md) on each one,
// reporting the mean fake-probability across frames. Reuses the already-loaded model
// instance (owned by AnalyzerRegistry) rather than loading the ~1.2GB CLIP backbone a
// second time.
class VideoAiFrameAnalyzer : public IAnalyzer {
public:
    explicit VideoAiFrameAnalyzer(const AiGeneratedModelAnalyzer* imageClassifier);

    std::string id() const override { return "ai-model:video-frame-classifier"; }
    std::string humanLabel() const override { return "Per-Frame AI-Image Classifier"; }
    std::vector<std::string> supportedMimeTypes() const override { return {"video/mp4"}; }
    bool isAvailable() const override;
    Evidence analyze(const AnalysisInput& input) const override;

private:
    const AiGeneratedModelAnalyzer* imageClassifier_;
};

} // namespace fakede
