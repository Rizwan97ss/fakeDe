#include "AnalysisController.h"

#include <drogon/MultiPart.h>

using namespace drogon;

namespace fakede {

AnalysisController::AnalysisController(std::shared_ptr<AnalyzerRegistry> registry,
                                        std::shared_ptr<JobStore> jobStore)
    : registry_(std::move(registry)), jobStore_(std::move(jobStore)) {}

void AnalysisController::handleAnalyze(const HttpRequestPtr& req,
                                        std::function<void(const HttpResponsePtr&)>&& callback) const {
    MultiPartParser parser;
    if (parser.parse(req) != 0 || parser.getFiles().empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(nlohmann::json{
            {"error", "Expected multipart/form-data with at least one file field named 'file'."}}
                          .dump());
        callback(resp);
        return;
    }

    const auto& file = parser.getFiles()[0];

    AnalysisInput input;
    input.fileName = file.getFileName();
    std::string_view content = file.fileContent();
    input.bytes.assign(content.begin(), content.end());
    input.mimeType = fileTypeSniffer_.detectMimeType(input.bytes);

    const auto analyzers = registry_->analyzersFor(input.mimeType);

    if (analyzers.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k415UnsupportedMediaType);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(nlohmann::json{
            {"error", "No analyzer available for detected type: " + input.mimeType},
            {"detectedMimeType", input.mimeType}}
                          .dump());
        callback(resp);
        return;
    }

    std::vector<Evidence> evidence;
    evidence.reserve(analyzers.size());
    for (const auto* analyzer : analyzers) {
        evidence.push_back(analyzer->analyze(input));
    }

    const Verdict verdict = fusionEngine_.fuse(std::move(evidence));
    const nlohmann::json verdictJson = verdict.toJson();
    const std::string verdictStr = verdictJson.dump();

    const std::string id = jobStore_->saveResult(input.fileName, input.mimeType, verdictStr);

    nlohmann::json responseJson = verdictJson;
    responseJson["id"] = id;
    responseJson["fileName"] = input.fileName;
    responseJson["detectedMimeType"] = input.mimeType;

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setBody(responseJson.dump());
    callback(resp);
}

void AnalysisController::handleGetResult(const HttpRequestPtr&,
                                          std::function<void(const HttpResponsePtr&)>&& callback,
                                          std::string id) const {
    const auto verdictJson = jobStore_->getResultJson(id);
    if (!verdictJson) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k404NotFound);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(R"({"error":"result not found"})");
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setBody(*verdictJson);
    callback(resp);
}

} // namespace fakede
