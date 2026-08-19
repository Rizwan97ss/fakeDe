#pragma once

#include "core/IAnalyzer.h"

namespace fakede {

// Frame-level approximation of jitter (cycle-to-cycle pitch-period variation) and
// shimmer (amplitude variation) - classical voice-quality forensic measures. Real
// human speech has natural micro-instability in pitch and loudness; older-generation
// TTS/vocoder-based synthesis is often unnaturally smooth by comparison. This is a
// weaker signal against modern neural voice synthesis, which can model natural jitter
// well - confidence is capped accordingly, same spirit as the text analyzers.
class VoiceNaturalnessAnalyzer : public IAnalyzer {
public:
    std::string id() const override { return "voice-naturalness"; }
    std::string humanLabel() const override { return "Voice Jitter/Shimmer Analysis"; }
    std::vector<std::string> supportedMimeTypes() const override { return {"audio/wav"}; }
    bool isAvailable() const override { return true; }
    Evidence analyze(const AnalysisInput& input) const override;
};

} // namespace fakede
