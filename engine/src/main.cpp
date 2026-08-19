#include <drogon/drogon.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#include "api/AnalysisController.h"
#include "core/AnalyzerRegistry.h"
#include "core/JobStore.h"

namespace {

std::string envOr(const char* name, std::string fallback) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : fallback;
}

void addCorsHeaders(const drogon::HttpResponsePtr& resp) {
    // Development-mode CORS: the React dev server (Vite, typically :5173) runs on a
    // different origin than the engine (:8080). Tighten this to a specific origin
    // allowlist before any production deployment.
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

} // namespace

int main() {
    const std::string modelsDir = envOr("FAKEDE_MODELS_DIR", "models");
    const std::string dbPath = envOr("FAKEDE_DB_PATH", "fakede.sqlite3");
    const uint16_t port = static_cast<uint16_t>(std::stoi(envOr("FAKEDE_PORT", "8080")));

    auto registry = std::make_shared<fakede::AnalyzerRegistry>(fakede::buildDefaultRegistry(modelsDir));
    auto jobStore = std::make_shared<fakede::JobStore>(dbPath);
    auto controller = std::make_shared<fakede::AnalysisController>(registry, jobStore);

    std::cout << "fakede engine starting on port " << port << "\n";
    std::cout << "models dir: " << std::filesystem::absolute(modelsDir) << "\n";
    for (const auto& analyzer : registry->all()) {
        std::cout << "  analyzer '" << analyzer->id() << "': "
                  << (analyzer->isAvailable() ? "available" : "UNAVAILABLE (missing assets)") << "\n";
    }

    drogon::app().registerPostHandlingAdvice(
        [](const drogon::HttpRequestPtr&, const drogon::HttpResponsePtr& resp) { addCorsHeaders(resp); });

    drogon::app().registerHandler(
        "/api/v1/analyze",
        [controller](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            controller->handleAnalyze(req, std::move(cb));
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/api/v1/analyze",
        [](const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            addCorsHeaders(resp);
            cb(resp);
        },
        {drogon::Options});

    // Drogon's direct registerHandler() uses numbered placeholders ({1}, {2}, ...),
    // not named ones - the trailing `std::string id` binds positionally to {1}.
    drogon::app().registerHandler(
        "/api/v1/analyze/{1}",
        [controller](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     std::string id) { controller->handleGetResult(req, std::move(cb), std::move(id)); },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/health",
        [](const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"status":"ok"})");
            cb(resp);
        },
        {drogon::Get});

    drogon::app().addListener("0.0.0.0", port);
    drogon::app().setThreadNum(0); // 0 = one IO loop per CPU core
    drogon::app().run();

    return 0;
}
