#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace fakede {

struct DecodedAudio {
    std::vector<float> monoSamples; // downmixed, range approx [-1, 1]
    uint32_t sampleRate = 0;
};

// Thin wrapper around dr_wav (engine/third_party/dr_wav.h, public domain/MIT-0 single
// header - chosen specifically to avoid another vcpkg autotools-style build; see
// docs/ARCHITECTURE.md for why that class of dependency has been a real problem here).
// WAV only for now - the least lossy common format and adequate for Phase 3's
// forensic analyzers, which want a clean, uncompressed waveform anyway.
class AudioDecoder {
public:
    static std::optional<DecodedAudio> decode(const std::vector<uint8_t>& bytes);
};

} // namespace fakede
