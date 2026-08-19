#pragma once

#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace fakede {

// One row of the analysis-history listing - deliberately just enough for a
// dashboard table (id, name, verdict, timestamp), not the full Verdict/evidence
// breakdown (fetch that separately via getResult when a row is opened).
struct AnalysisSummary {
    std::string id;
    std::string fileName;
    std::string mimeType;
    std::string overallLabel;
    double overallScore = 0.0;
    std::string createdAt;
};

// A single stored result, re-fetched by id. `verdictJson` is the already-serialized
// Verdict; fileName/mimeType are carried alongside it so GET /analyze/{id} can return
// the same shape (id + fileName + detectedMimeType + verdict fields) as the original
// POST /analyze response, instead of a bare Verdict with no file identity.
struct StoredResult {
    std::string fileName;
    std::string mimeType;
    std::string verdictJson;
};

// Analysis-history store: one row per completed analysis, keyed by id.
class JobStore {
public:
    explicit JobStore(const std::string& dbPath);
    ~JobStore();

    JobStore(const JobStore&) = delete;
    JobStore& operator=(const JobStore&) = delete;

    // `verdictJson` is the already-serialized Verdict::toJson().dump(); `overallLabel`/
    // `overallScore` are duplicated out of it into their own columns so listRecent()
    // doesn't need to parse every row's JSON just to render a history table.
    std::string saveResult(const std::string& fileName, const std::string& mimeType,
                            const std::string& verdictJson, const std::string& overallLabel,
                            double overallScore);

    std::optional<StoredResult> getResult(const std::string& id) const;

    std::vector<AnalysisSummary> listRecent(int limit) const;

private:
    sqlite3* db_ = nullptr;
    void migrate();
};

} // namespace fakede
