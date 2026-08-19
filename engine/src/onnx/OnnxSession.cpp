#include "OnnxSession.h"

#include <onnxruntime_cxx_api.h>

#include <filesystem>
#include <iostream>

namespace fakede {

OnnxSession::OnnxSession(const std::string& modelPath) {
    if (!std::filesystem::exists(modelPath)) {
        // Not an error: models are fetched separately (scripts/fetch_models.ps1) and may
        // simply not be present yet on a fresh checkout. isLoaded() reports this.
        return;
    }

    try {
        env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "fakede");

        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        const std::wstring widePath(modelPath.begin(), modelPath.end());
        session_ = std::make_unique<Ort::Session>(*env_, widePath.c_str(), options);
#else
        session_ = std::make_unique<Ort::Session>(*env_, modelPath.c_str(), options);
#endif

        Ort::AllocatorWithDefaultOptions allocator;
        auto inputNamePtr = session_->GetInputNameAllocated(0, allocator);
        auto outputNamePtr = session_->GetOutputNameAllocated(0, allocator);
        inputName_ = inputNamePtr.get();
        outputName_ = outputNamePtr.get();
    } catch (const Ort::Exception& ex) {
        std::cerr << "OnnxSession: failed to load model at " << modelPath << ": " << ex.what() << std::endl;
        session_.reset();
        env_.reset();
    }
}

OnnxSession::~OnnxSession() = default;

std::optional<std::vector<float>> OnnxSession::runSingleInputOutput(
    const std::vector<float>& inputData, const std::vector<int64_t>& inputShape) const {
    if (!isLoaded()) {
        return std::nullopt;
    }

    try {
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memoryInfo, const_cast<float*>(inputData.data()), inputData.size(), inputShape.data(), inputShape.size());

        const char* inputNames[] = {inputName_.c_str()};
        const char* outputNames[] = {outputName_.c_str()};

        auto outputTensors = session_->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
        if (outputTensors.empty() || !outputTensors[0].IsTensor()) {
            return std::nullopt;
        }

        const float* outData = outputTensors[0].GetTensorData<float>();
        const auto outShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        int64_t total = 1;
        for (auto dim : outShape) total *= std::max<int64_t>(dim, 1);

        return std::vector<float>(outData, outData + total);
    } catch (const Ort::Exception& ex) {
        std::cerr << "OnnxSession: inference failed: " << ex.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace fakede
