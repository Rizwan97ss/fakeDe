#include "JobStore.h"

#include <sqlite3.h>

#include <chrono>
#include <random>
#include <sstream>
#include <stdexcept>

namespace fakede {

namespace {
std::string makeId() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << dist(rng) << dist(rng);
    return oss.str();
}
} // namespace

JobStore::JobStore(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("failed to open sqlite db at " + dbPath);
    }
    migrate();
}

JobStore::~JobStore() {
    if (db_) sqlite3_close(db_);
}

void JobStore::migrate() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS results ("
        "  id TEXT PRIMARY KEY,"
        "  file_name TEXT NOT NULL,"
        "  mime_type TEXT NOT NULL,"
        "  verdict_json TEXT NOT NULL,"
        "  created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");";
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown sqlite error";
        sqlite3_free(errMsg);
        throw std::runtime_error("failed to migrate sqlite db: " + msg);
    }
}

std::string JobStore::saveResult(const std::string& fileName, const std::string& mimeType,
                                  const std::string& verdictJson) {
    const std::string id = makeId();
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO results (id, file_name, mime_type, verdict_json) VALUES (?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("failed to prepare insert statement");
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, mimeType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, verdictJson.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("failed to insert analysis result");
    }
    return id;
}

std::optional<std::string> JobStore::getResultJson(const std::string& id) const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT verdict_json FROM results WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<std::string> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            result = std::string(reinterpret_cast<const char*>(text));
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

} // namespace fakede
