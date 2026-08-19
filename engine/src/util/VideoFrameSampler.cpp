#include "VideoFrameSampler.h"

#include <opencv2/videoio.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

namespace fakede {

std::optional<VideoFrameSample> VideoFrameSampler::sample(const std::vector<uint8_t>& bytes, int maxFrames) {
    namespace fs = std::filesystem;

    static thread_local std::mt19937_64 rng{std::random_device{}()};
    const fs::path tempPath = fs::temp_directory_path() / ("fakede_video_" + std::to_string(rng()) + ".mp4");

    {
        std::ofstream out(tempPath, std::ios::binary);
        if (!out) return std::nullopt;
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    // Explicit backend rather than relying on cv::VideoCapture's default priority
    // order - Media Foundation is the native Windows decoder this project relies on
    // instead of pulling in FFmpeg via vcpkg (see docs/ARCHITECTURE.md).
    cv::VideoCapture cap(tempPath.string(), cv::CAP_MSMF);
    if (!cap.isOpened()) {
        std::error_code ec;
        fs::remove(tempPath, ec);
        return std::nullopt;
    }

    VideoFrameSample result;
    result.fps = cap.get(cv::CAP_PROP_FPS);
    const double frameCount = cap.get(cv::CAP_PROP_FRAME_COUNT);
    result.durationSeconds = (result.fps > 0.0 && frameCount > 0.0) ? frameCount / result.fps : 0.0;

    if (frameCount > 0.0 && maxFrames > 0) {
        const int totalFrames = static_cast<int>(frameCount);
        const int step = std::max(1, totalFrames / maxFrames);
        for (int idx = 0; idx < totalFrames && static_cast<int>(result.frames.size()) < maxFrames; idx += step) {
            cap.set(cv::CAP_PROP_POS_FRAMES, idx);
            cv::Mat frame;
            if (cap.read(frame) && !frame.empty()) {
                result.frames.push_back(frame.clone());
            }
        }
    } else {
        // Some containers/codecs don't report a reliable frame count - fall back to
        // reading sequentially.
        cv::Mat frame;
        while (static_cast<int>(result.frames.size()) < maxFrames && cap.read(frame)) {
            if (!frame.empty()) result.frames.push_back(frame.clone());
        }
    }

    cap.release();
    std::error_code ec;
    fs::remove(tempPath, ec);

    if (result.frames.empty()) return std::nullopt;
    return result;
}

} // namespace fakede
