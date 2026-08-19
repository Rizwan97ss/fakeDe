#include "AudioDecoder.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

namespace fakede {

std::optional<DecodedAudio> AudioDecoder::decode(const std::vector<uint8_t>& bytes) {
    drwav wav;
    if (!drwav_init_memory(&wav, bytes.data(), bytes.size(), nullptr)) {
        return std::nullopt;
    }

    if (wav.totalPCMFrameCount == 0 || wav.channels == 0) {
        drwav_uninit(&wav);
        return std::nullopt;
    }

    std::vector<float> interleaved(static_cast<size_t>(wav.totalPCMFrameCount) * wav.channels);
    const drwav_uint64 framesRead = drwav_read_pcm_frames_f32(&wav, wav.totalPCMFrameCount, interleaved.data());

    DecodedAudio result;
    result.sampleRate = wav.sampleRate;
    result.monoSamples.resize(framesRead);

    if (wav.channels == 1) {
        result.monoSamples.assign(interleaved.begin(), interleaved.begin() + framesRead);
    } else {
        for (drwav_uint64 i = 0; i < framesRead; ++i) {
            float sum = 0.0f;
            for (uint32_t c = 0; c < wav.channels; ++c) {
                sum += interleaved[i * wav.channels + c];
            }
            result.monoSamples[i] = sum / static_cast<float>(wav.channels);
        }
    }

    drwav_uninit(&wav);
    return result;
}

} // namespace fakede
