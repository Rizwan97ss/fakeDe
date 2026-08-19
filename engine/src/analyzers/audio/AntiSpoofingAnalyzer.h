#pragma once

#include <memory>
#include <string>

#include "core/IAnalyzer.h"
#include "onnx/OnnxSession.h"

namespace fakede {

// Runs a pretrained synthetic-speech/anti-spoofing classifier (RawNet2, Tak et al.,
// arXiv:2011.01108 - see docs/model-sourcing.md for exact source/license/conversion
// notes) directly on the raw waveform, no hand-crafted features. Same graceful-
// degradation pattern as every other ML analyzer in this codebase: isAvailable() is
// false until the ONNX weights are actually fetched, and the rest of the pipeline
// works fine without it.
class AntiSpoofingAnalyzer : public IAnalyzer {
public:
    explicit AntiSpoofingAnalyzer(const std::string& modelPath);

    std::string id() const override { return "ai-model:audio-antispoofing"; }
    std::string humanLabel() const override { return "Synthetic Speech Classifier"; }
    std::vector<std::string> supportedMimeTypes() const override { return {"audio/wav"}; }
    bool isAvailable() const override { return session_->isLoaded(); }
    Evidence analyze(const AnalysisInput& input) const override;

private:
    std::unique_ptr<OnnxSession> session_;
};

} // namespace fakede
