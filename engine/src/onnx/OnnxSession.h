#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Ort {
class Env;
class Session;
} // namespace Ort

namespace fakede {

// Thin RAII wrapper around an ONNX Runtime session for a single model. Every ML-backed
// analyzer owns one of these. Construction never throws on a missing/corrupt model file
// — it leaves the session unloaded so IAnalyzer::isAvailable() can report false and the
// pipeline degrades gracefully instead of crashing the process over a missing weight file.
class OnnxSession {
public:
    // `modelPath` is a .onnx file. If it doesn't exist or fails to load, isLoaded() is false.
    explicit OnnxSession(const std::string& modelPath);
    ~OnnxSession();

    OnnxSession(const OnnxSession&) = delete;
    OnnxSession& operator=(const OnnxSession&) = delete;

    bool isLoaded() const { return session_ != nullptr; }

    // Runs the single-input, single-output common case: an NCHW float32 tensor in,
    // a float32 vector out (e.g. class logits/probabilities). Returns nullopt if the
    // session isn't loaded or inference fails.
    std::optional<std::vector<float>> runSingleInputOutput(const std::vector<float>& inputData,
                                                             const std::vector<int64_t>& inputShape) const;

private:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::string inputName_;
    std::string outputName_;
};

} // namespace fakede
