#pragma once

#include <optional>
#include <string>

struct sqlite3;

namespace fakede {

// Minimal analysis-history store: one row per completed analysis, keyed by id.
// Deliberately not a full dashboard/query API yet — just enough to let a client
// re-fetch a result by id after the initial synchronous response.
class JobStore {
public:
    explicit JobStore(const std::string& dbPath);
    ~JobStore();

    JobStore(const JobStore&) = delete;
    JobStore& operator=(const JobStore&) = delete;

    // `verdictJson` is the already-serialized Verdict::toJson().dump().
    std::string saveResult(const std::string& fileName, const std::string& mimeType,
                            const std::string& verdictJson);

    std::optional<std::string> getResultJson(const std::string& id) const;

private:
    sqlite3* db_ = nullptr;
    void migrate();
};

} // namespace fakede
