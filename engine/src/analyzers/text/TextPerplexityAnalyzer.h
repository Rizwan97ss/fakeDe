#pragma once

#include <memory>
#include <string>

#include "Gpt2Tokenizer.h"
#include "core/IAnalyzer.h"
#include "onnx/OnnxSession.h"

namespace fakede {

// Scores text via a single causal LM (GPT-2 small, ONNX) rather than the ratio-of-two-
// models approach used by Binoculars/Fast-DetectGPT (see docs/ROADMAP.md - that's
// documented future work, not implemented here). This is closer to the classic
// GPTZero-era "perplexity + burstiness" method: text the model finds very predictable
// (low perplexity) and evenly-so (low variance in per-token surprisal) scores as more
// likely AI-generated. Text detection is the least reliable domain in this whole
// product (see docs/model-sourcing.md) and this analyzer's confidence is capped low
// accordingly, regardless of how extreme its inputs are.
class TextPerplexityAnalyzer : public IAnalyzer {
public:
    TextPerplexityAnalyzer(const std::string& modelPath, const std::string& vocabPath, const std::string& mergesPath);

    std::string id() const override { return "ai-model:text-perplexity"; }
    std::string humanLabel() const override { return "Language Model Perplexity"; }
    std::vector<std::string> supportedMimeTypes() const override { return {"text/plain"}; }
    bool isAvailable() const override { return session_->isLoaded() && tokenizer_->isLoaded(); }
    Evidence analyze(const AnalysisInput& input) const override;

private:
    std::unique_ptr<OnnxSession> session_;
    std::unique_ptr<Gpt2Tokenizer> tokenizer_;
};

} // namespace fakede
