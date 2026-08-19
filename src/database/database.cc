#include "database/database.h"

#include <cstring>
#include <utility>

#include "third_party/sqlite/sqlite3.h"

namespace buried {

namespace {

const char kCreateTableSql[] =
    "CREATE TABLE IF NOT EXISTS buried_data ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "priority INTEGER NOT NULL,"
    "timestamp INTEGER NOT NULL,"
    "content BLOB NOT NULL);";

}  // namespace

class BuriedDbImpl {
 public:
  explicit BuriedDbImpl(std::string db_path) : db_path_(std::move(db_path)) {
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
      if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
      }
      return;
    }
    sqlite3_exec(db_, kCreateTableSql, nullptr, nullptr, nullptr);
  }

  ~BuriedDbImpl() {
    if (db_) {
      sqlite3_close(db_);
    }
  }

  void InsertData(const BuriedDb::Data& data) {
    if (!db_) {
      return;
    }
    const char* sql = "INSERT INTO buried_data (priority, timestamp, content) "
                      "VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      return;
    }
    sqlite3_bind_int(stmt, 1, data.priority);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(data.timestamp));
    sqlite3_bind_blob(stmt, 3, data.content.data(),
                      static_cast<int>(data.content.size()), SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  void DeleteData(const BuriedDb::Data& data) {
    if (!db_) {
      return;
    }
    const char* sql = "DELETE FROM buried_data WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      return;
    }
    sqlite3_bind_int(stmt, 1, data.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }

  void DeleteDatas(const std::vector<BuriedDb::Data>& datas) {
    if (!db_ || datas.empty()) {
      return;
    }
    sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);
    bool ok = true;
    for (const auto& data : datas) {
      const char* sql = "DELETE FROM buried_data WHERE id = ?;";
      sqlite3_stmt* stmt = nullptr;
      if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        ok = false;
        break;
      }
      sqlite3_bind_int(stmt, 1, data.id);
      if (sqlite3_step(stmt) != SQLITE_DONE) {
        ok = false;
      }
      sqlite3_finalize(stmt);
      if (!ok) {
        break;
      }
    }
    sqlite3_exec(db_, ok ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, nullptr);
  }

  std::vector<BuriedDb::Data> QueryData(int32_t limit) {
    std::vector<BuriedDb::Data> result;
    if (!db_) {
      return result;
    }
    const char* sql =
        "SELECT id, priority, timestamp, content FROM buried_data "
        "ORDER BY priority DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
      return result;
    }
    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      BuriedDb::Data data;
      data.id = sqlite3_column_int(stmt, 0);
      data.priority = sqlite3_column_int(stmt, 1);
      data.timestamp = static_cast<uint64_t>(sqlite3_column_int64(stmt, 2));
      const char* blob =
          static_cast<const char*>(sqlite3_column_blob(stmt, 3));
      int blob_size = sqlite3_column_bytes(stmt, 3);
      data.content.assign(blob, blob + blob_size);
      result.push_back(std::move(data));
    }
    sqlite3_finalize(stmt);
    return result;
  }

 private:
  std::string db_path_;
  sqlite3* db_ = nullptr;
};

BuriedDb::BuriedDb(std::string db_path)
    : impl_{std::make_unique<BuriedDbImpl>(std::move(db_path))} {}

BuriedDb::~BuriedDb() {}

void BuriedDb::InsertData(const Data& data) { impl_->InsertData(data); }

void BuriedDb::DeleteData(const Data& data) { impl_->DeleteData(data); }

void BuriedDb::DeleteDatas(const std::vector<Data>& datas) {
  impl_->DeleteDatas(datas);
}

std::vector<BuriedDb::Data> BuriedDb::QueryData(int32_t limit) {
  return impl_->QueryData(limit);
}

}  // namespace buried
