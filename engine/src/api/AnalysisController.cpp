#include "AnalysisController.h"

#include <drogon/MultiPart.h>

#include <algorithm>

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

    const std::string id =
        jobStore_->saveResult(input.fileName, input.mimeType, verdictStr, toString(verdict.overallLabel),
                               verdict.overallScore);

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
    const auto stored = jobStore_->getResult(id);
    if (!stored) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k404NotFound);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(R"({"error":"result not found"})");
        callback(resp);
        return;
    }

    // Re-attach the same id/fileName/detectedMimeType fields the original POST
    // /analyze response carried, so a result reopened from history (GET) has the same
    // shape the frontend's AnalysisResponse type expects as one freshly analyzed.
    nlohmann::json responseJson = nlohmann::json::parse(stored->verdictJson, nullptr, false);
    if (responseJson.is_discarded()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setBody(R"({"error":"stored result is corrupted"})");
        callback(resp);
        return;
    }
    responseJson["id"] = id;
    responseJson["fileName"] = stored->fileName;
    responseJson["detectedMimeType"] = stored->mimeType;

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setBody(responseJson.dump());
    callback(resp);
}

void AnalysisController::handleListRecent(const HttpRequestPtr& req,
                                           std::function<void(const HttpResponsePtr&)>&& callback) const {
    int limit = 20;
    const std::string& limitParam = req->getParameter("limit");
    if (!limitParam.empty()) {
        try {
            limit = std::stoi(limitParam);
        } catch (const std::exception&) {
            // keep default on unparsable input
        }
    }
    limit = std::clamp(limit, 1, 100);

    nlohmann::json items = nlohmann::json::array();
    for (const auto& row : jobStore_->listRecent(limit)) {
        items.push_back({
            {"id", row.id},
            {"fileName", row.fileName},
            {"mimeType", row.mimeType},
            {"overallLabel", row.overallLabel},
            {"overallScore", row.overallScore},
            {"createdAt", row.createdAt},
        });
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setBody(nlohmann::json{{"results", items}}.dump());
    callback(resp);
}

} // namespace fakede
