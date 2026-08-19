#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <memory>

#include "core/AnalyzerRegistry.h"
#include "core/FusionEngine.h"
#include "core/JobStore.h"
#include "util/FileTypeSniffer.h"

namespace fakede {

// Handles the two Phase-1 HTTP endpoints:
//   POST /api/v1/analyze       - multipart file upload, returns a full Verdict synchronously
//   GET  /api/v1/analyze/{id}  - re-fetch a previously computed Verdict by id
// Wired up directly (not through Drogon's macro-based HttpController) so routing is
// visible in one place (main.cpp) and this class stays plain, testable C++.
class AnalysisController {
public:
    AnalysisController(std::shared_ptr<AnalyzerRegistry> registry, std::shared_ptr<JobStore> jobStore);

    void handleAnalyze(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback) const;

    void handleGetResult(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          std::string id) const;

private:
    std::shared_ptr<AnalyzerRegistry> registry_;
    std::shared_ptr<JobStore> jobStore_;
    FusionEngine fusionEngine_;
    FileTypeSniffer fileTypeSniffer_;
};

} // namespace fakede
