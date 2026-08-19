#include "AntiSpoofingAnalyzer.h"

#include <algorithm>
#include <cmath>

#include "util/AudioDecoder.h"

namespace fakede {

namespace {
// Matches the standard ASVspoof2019/RawNet2 baseline convention: 16kHz, ~4-second
// (64600-sample) fixed-length input, built by tiling short clips or cropping long ones.
constexpr uint32_t kTargetSampleRate = 16000;
constexpr size_t kTargetLength = 64600;

std::vector<float> resampleLinear(const std::vector<float>& input, uint32_t srcRate, uint32_t dstRate) {
    if (srcRate == dstRate || srcRate == 0 || input.empty()) return input;
    const double ratio = static_cast<double>(dstRate) / srcRate;
    const size_t outLen = static_cast<size_t>(input.size() * ratio);
    std::vector<float> out(outLen);
    for (size_t i = 0; i < outLen; ++i) {
        const double srcPos = i / ratio;
        const size_t idx0 = static_cast<size_t>(srcPos);
        const size_t idx1 = std::min(idx0 + 1, input.size() - 1);
        const double frac = srcPos - idx0;
        out[i] = static_cast<float>(input[idx0] * (1.0 - frac) + input[idx1] * frac);
    }
    return out;
}

std::vector<float> padOrCrop(const std::vector<float>& input, size_t targetLen) {
    if (input.empty()) return std::vector<float>(targetLen, 0.0f);
    if (input.size() >= targetLen) {
        return std::vector<float>(input.begin(), input.begin() + targetLen);
    }
    std::vector<float> out;
    out.reserve(targetLen);
    while (out.size() < targetLen) {
        const size_t remaining = targetLen - out.size();
        const size_t toCopy = std::min(remaining, input.size());
        out.insert(out.end(), input.begin(), input.begin() + toCopy);
    }
    return out;
}

} // namespace

AntiSpoofingAnalyzer::AntiSpoofingAnalyzer(const std::string& modelPath)
    : session_(std::make_unique<OnnxSession>(modelPath)) {}

Evidence AntiSpoofingAnalyzer::analyze(const AnalysisInput& input) const {
    Evidence evidence;
    evidence.analyzerId = id();
    evidence.humanLabel = humanLabel();

    if (!session_->isLoaded()) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "The synthetic-voice-detection model isn't set up on this server yet, so this check was skipped.";
        return evidence;
    }

    const auto decoded = AudioDecoder::decode(input.bytes);
    if (!decoded) {
        evidence.score = 0.5;
        evidence.confidence = 0.0;
        evidence.explanation = "We couldn't process this audio for this check.";
        return evidence;
    }

    const std::vector<float> resampled = resampleLinear(decoded->monoSamples, decoded->sampleRate, kTargetSampleRate);
    const std::vector<float> fixedLength = padOrCrop(resampled, kTargetLength);

    const std::vector<int64_t> shape{1, static_cast<int64_t>(fixedLength.size())};
    const auto output = session_->runSingleInputOutput(fixedLength, shape);
    if (!output || output->empty()) {
        evidence.score = 0.5;
        evidence.confidence = 0.1;
        evidence.explanation = "This check didn't run properly on this audio.";
        return evidence;
    }

    double fakeProbability = 0.5;
    if (output->size() == 1) {
        fakeProbability = 1.0 / (1.0 + std::exp(-static_cast<double>((*output)[0])));
    } else {
        const float maxLogit = *std::max_element(output->begin(), output->end());
        double sumExp = 0.0;
        for (float v : *output) sumExp += std::exp(v - maxLogit);
        // ASVspoof label convention: index 0 = bonafide (real), index 1 = spoof (fake).
        const double bonafideProb = std::exp((*output)[0] - maxLogit) / sumExp;
        fakeProbability = 1.0 - bonafideProb;
    }

    evidence.score = std::clamp(fakeProbability, 0.0, 1.0);
    evidence.confidence = 0.8; // purpose-built classifier; high standalone confidence when loaded
    evidence.explanation = evidence.score > 0.6
        ? "We ran this audio through a model trained to catch AI-generated or cloned voices. It thinks this "
          "audio is very likely synthetic, not a real recorded voice."
        : "We ran this audio through a model trained to catch AI-generated or cloned voices. It thinks this "
          "is very likely a real recorded voice, not synthetic.";
    evidence.rawDetails["fakeProbability"] = fakeProbability;

    return evidence;
}

} // namespace fakede
