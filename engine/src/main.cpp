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

void addCorsHeaders(const drogon::HttpResponsePtr& resp, const std::string& allowedOrigin) {
    // FAKEDE_ALLOWED_ORIGIN defaults to "*" for local dev convenience (the Vite dev
    // server on :5173 talking to the engine on :8080). Set it to a specific origin
    // before any production deployment.
    resp->addHeader("Access-Control-Allow-Origin", allowedOrigin);
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
}

} // namespace

int main() {
#ifdef FAKEDE_DEFAULT_MODELS_DIR
    const std::string modelsDir = envOr("FAKEDE_MODELS_DIR", FAKEDE_DEFAULT_MODELS_DIR);
#else
    const std::string modelsDir = envOr("FAKEDE_MODELS_DIR", "models");
#endif
    const std::string dbPath = envOr("FAKEDE_DB_PATH", "fakede.sqlite3");
    const uint16_t port = static_cast<uint16_t>(std::stoi(envOr("FAKEDE_PORT", "8080")));
    const std::string allowedOrigin = envOr("FAKEDE_ALLOWED_ORIGIN", "*");
    const std::string apiKey = envOr("FAKEDE_API_KEY", "");

    auto registry = std::make_shared<fakede::AnalyzerRegistry>(fakede::buildDefaultRegistry(modelsDir));
    auto jobStore = std::make_shared<fakede::JobStore>(dbPath);
    auto controller = std::make_shared<fakede::AnalysisController>(registry, jobStore);

    std::cout << "fakede engine starting on port " << port << "\n";
    std::cout << "models dir: " << std::filesystem::absolute(modelsDir) << "\n";
    for (const auto& analyzer : registry->all()) {
        std::cout << "  analyzer '" << analyzer->id() << "': "
                  << (analyzer->isAvailable() ? "available" : "UNAVAILABLE (missing assets)") << "\n";
    }

    // Drogon defaults to a 1MB max request body, which would silently reject most
    // real photos/audio clips and essentially all video files. 200MB comfortably
    // covers this project's supported file types without leaving the limit unbounded.
    drogon::app().setClientMaxBodySize(200 * 1024 * 1024);

    drogon::app().registerPostHandlingAdvice(
        [allowedOrigin](const drogon::HttpRequestPtr&, const drogon::HttpResponsePtr& resp) {
            addCorsHeaders(resp, allowedOrigin);
        });

    // Minimal shared-secret protection: if FAKEDE_API_KEY is set, every /api/v1/*
    // request except /health and CORS preflight OPTIONS must carry a matching
    // X-API-Key header. This is deliberately NOT a user-account system (no sessions,
    // no per-user identity) - it's a single-secret gate meant to keep a deployed
    // engine from being hit by anonymous public traffic. A real multi-user auth
    // system remains future work (see docs/ROADMAP.md, Phase 5).
    if (!apiKey.empty()) {
        drogon::app().registerPreRoutingAdvice(
            [apiKey](const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& acb,
                     drogon::AdviceChainCallback&& accb) {
                if (req->method() == drogon::Options || req->path() == "/api/v1/health") {
                    accb();
                    return;
                }
                if (req->getHeader("X-API-Key") == apiKey) {
                    accb();
                    return;
                }
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k401Unauthorized);
                resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
                resp->setBody(R"({"error":"missing or invalid X-API-Key header"})");
                acb(resp);
            });
        std::cout << "API key auth: ENABLED (X-API-Key header required on /api/v1/* except /health)\n";
    } else {
        std::cout << "API key auth: disabled (set FAKEDE_API_KEY to enable)\n";
    }

    drogon::app().registerHandler(
        "/api/v1/analyze",
        [controller](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            controller->handleAnalyze(req, std::move(cb));
        },
        {drogon::Post});

    // A shared OPTIONS handler for every /api/v1/* CORS preflight - registered per
    // path since Drogon's direct registerHandler() doesn't support a path wildcard.
    // Needed cross-origin because a custom X-API-Key header triggers a preflight even
    // on GET requests. In dev, Vite's proxy makes requests same-origin so this path is
    // never exercised there, but it matters for any real cross-origin deployment.
    auto optionsHandler = [](const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        cb(resp);
    };
    // The {1} path needs its own handler with a matching trailing parameter - Drogon
    // binds placeholders to trailing function parameters by count, independent of
    // HTTP method, so reusing the zero-parameter optionsHandler here would mismatch.
    auto optionsHandlerWithId = [](const drogon::HttpRequestPtr&,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& cb, const std::string&) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        cb(resp);
    };
    drogon::app().registerHandler("/api/v1/analyze", optionsHandler, {drogon::Options});
    drogon::app().registerHandler("/api/v1/analyze/{1}", optionsHandlerWithId, {drogon::Options});
    drogon::app().registerHandler("/api/v1/analyses", optionsHandler, {drogon::Options});

    // Drogon's direct registerHandler() uses numbered placeholders ({1}, {2}, ...),
    // not named ones - the trailing `std::string id` binds positionally to {1}.
    drogon::app().registerHandler(
        "/api/v1/analyze/{1}",
        [controller](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb,
                     std::string id) { controller->handleGetResult(req, std::move(cb), std::move(id)); },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/analyses",
        [controller](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            controller->handleListRecent(req, std::move(cb));
        },
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
