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
        "  overall_label TEXT NOT NULL DEFAULT '',"
        "  overall_score REAL NOT NULL DEFAULT 0,"
        "  sha256_hex TEXT NOT NULL DEFAULT '',"
        "  blake3_hex TEXT NOT NULL DEFAULT '',"
        "  created_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");";
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown sqlite error";
        sqlite3_free(errMsg);
        throw std::runtime_error("failed to migrate sqlite db: " + msg);
    }
    // Best-effort upgrade for databases created before these columns existed (Phases
    // 1-4). Fails harmlessly with "duplicate column" on a fresh CREATE TABLE above,
    // or on a database that's already been upgraded - both ignored on purpose.
    sqlite3_exec(db_, "ALTER TABLE results ADD COLUMN overall_label TEXT NOT NULL DEFAULT '';", nullptr, nullptr,
                 nullptr);
    sqlite3_exec(db_, "ALTER TABLE results ADD COLUMN overall_score REAL NOT NULL DEFAULT 0;", nullptr, nullptr,
                 nullptr);
    sqlite3_exec(db_, "ALTER TABLE results ADD COLUMN sha256_hex TEXT NOT NULL DEFAULT '';", nullptr, nullptr,
                 nullptr);
    sqlite3_exec(db_, "ALTER TABLE results ADD COLUMN blake3_hex TEXT NOT NULL DEFAULT '';", nullptr, nullptr,
                 nullptr);
}

std::string JobStore::saveResult(const std::string& fileName, const std::string& mimeType,
                                  const std::string& sha256Hex, const std::string& blake3Hex,
                                  const std::string& verdictJson, const std::string& overallLabel,
                                  double overallScore) {
    const std::string id = makeId();
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO results (id, file_name, mime_type, verdict_json, overall_label, overall_score, "
        "sha256_hex, blake3_hex) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("failed to prepare insert statement");
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, fileName.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, mimeType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, verdictJson.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, overallLabel.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, overallScore);
    sqlite3_bind_text(stmt, 7, sha256Hex.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, blake3Hex.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("failed to insert analysis result");
    }
    return id;
}

std::optional<StoredResult> JobStore::getResult(const std::string& id) const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT file_name, mime_type, sha256_hex, blake3_hex, verdict_json FROM results WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<StoredResult> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        auto textOrEmpty = [stmt](int col) {
            const unsigned char* text = sqlite3_column_text(stmt, col);
            return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
        };
        result = StoredResult{textOrEmpty(0), textOrEmpty(1), textOrEmpty(2), textOrEmpty(3), textOrEmpty(4)};
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<AnalysisSummary> JobStore::listRecent(int limit) const {
    std::vector<AnalysisSummary> out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, file_name, mime_type, overall_label, overall_score, created_at "
        "FROM results ORDER BY created_at DESC, rowid DESC LIMIT ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return out;
    }
    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AnalysisSummary summary;
        auto textOrEmpty = [stmt](int col) {
            const unsigned char* text = sqlite3_column_text(stmt, col);
            return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
        };
        summary.id = textOrEmpty(0);
        summary.fileName = textOrEmpty(1);
        summary.mimeType = textOrEmpty(2);
        summary.overallLabel = textOrEmpty(3);
        summary.overallScore = sqlite3_column_double(stmt, 4);
        summary.createdAt = textOrEmpty(5);
        out.push_back(std::move(summary));
    }
    sqlite3_finalize(stmt);
    return out;
}

} // namespace fakede
