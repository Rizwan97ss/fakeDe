#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <opencv2/core.hpp>

namespace fakede {

struct VideoFrameSample {
    std::vector<cv::Mat> frames; // BGR, evenly spaced across the video
    double durationSeconds = 0.0;
    double fps = 0.0;
};

// Wraps cv::VideoCapture (Windows Media Foundation backend - see docs/ARCHITECTURE.md
// for why this project uses that instead of pulling in FFmpeg via vcpkg). VideoCapture
// needs a real file for container demuxing, so this writes the input bytes to a temp
// file, samples, and cleans up before returning.
class VideoFrameSampler {
public:
    static std::optional<VideoFrameSample> sample(const std::vector<uint8_t>& bytes, int maxFrames);
};

} // namespace fakede
