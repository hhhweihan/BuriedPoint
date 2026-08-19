#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "buried_core.h"
#include "common/common_service.h"
#include "crypt/crypt.h"
#include "database/database.h"
#include "fs_util.h"
#include "include/buried.h"

namespace {

using namespace buried;

std::string MakeTempDir() {
  std::string dir = "/tmp/buried_gtest_" + CommonService::GetRandomId();
  CreateDirectories(dir);
  return dir;
}

void RemoveAll(const std::string& path) {
  DIR* d = opendir(path.c_str());
  if (d) {
    struct dirent* ent = nullptr;
    while ((ent = readdir(d)) != nullptr) {
      std::string name = ent->d_name;
      if (name == "." || name == "..") {
        continue;
      }
      std::string full = path + "/" + name;
      struct stat st;
      if (lstat(full.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
          RemoveAll(full);
        } else {
          ::remove(full.c_str());
        }
      }
    }
    closedir(d);
  }
  ::rmdir(path.c_str());
}

class TempDirGuard {
 public:
  TempDirGuard() : path_(MakeTempDir()) {}
  ~TempDirGuard() { RemoveAll(path_); }
  const std::string& path() const { return path_; }

 private:
  std::string path_;
};

Buried::Config MakeConfig() {
  Buried::Config config;
  config.host = "127.0.0.1";
  config.port = "80";
  config.topic = "/api/v1/report";
  config.user_id = "user_test";
  config.app_version = "1.0.0";
  config.app_name = "BuriedTest";
  config.custom_data = "{\"channel\":\"unittest\"}";
  return config;
}

TEST(PathUtilTest, PathExists) {
  EXPECT_FALSE(PathExists("/nonexistent_path_xyz_12345"));
  EXPECT_FALSE(PathExists(""));
  EXPECT_TRUE(PathExists("/tmp"));
}

TEST(PathUtilTest, CreateDirectories) {
  EXPECT_FALSE(CreateDirectories(""));
  TempDirGuard guard;
  std::string nested = guard.path() + "/a/b/c";
  EXPECT_TRUE(CreateDirectories(nested));
  EXPECT_TRUE(PathExists(nested));
}

TEST(CommonServiceTest, DeviceInfoFilled) {
  CommonService service;
  EXPECT_FALSE(service.system_version.empty());
  EXPECT_FALSE(service.device_id.empty());
  EXPECT_FALSE(service.device_name.empty());
  EXPECT_FALSE(service.buried_version.empty());
  EXPECT_FALSE(service.lifecycle_id.empty());
}

TEST(CommonServiceTest, NowDateFormat) {
  std::string date = CommonService::GetNowDate();
  ASSERT_EQ(date.size(), 19u);
  EXPECT_EQ(date[4], '-');
  EXPECT_EQ(date[7], '-');
  EXPECT_EQ(date[10], ' ');
  EXPECT_EQ(date[13], ':');
  EXPECT_EQ(date[16], ':');
}

TEST(CommonServiceTest, RandomId) {
  std::string id1 = CommonService::GetRandomId();
  std::string id2 = CommonService::GetRandomId();
  EXPECT_EQ(id1.size(), 32u);
  EXPECT_NE(id1, id2);
  for (char c : id1) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z'));
  }
}

TEST(AESCryptTest, GetKeyDeterministic) {
  std::string key1 = AESCrypt::GetKey("salt", "password");
  std::string key2 = AESCrypt::GetKey("salt", "password");
  std::string key3 = AESCrypt::GetKey("salt", "other_password");
  EXPECT_EQ(key1.size(), 32u);
  EXPECT_EQ(key1, key2);
  EXPECT_NE(key1, key3);
}

TEST(AESCryptTest, EncryptDecryptRoundtrip) {
  AESCrypt crypt(AESCrypt::GetKey("salt", "password"));

  std::string plain = "hello buried point";
  std::string cipher = crypt.Encrypt(plain);
  EXPECT_FALSE(cipher.empty());
  EXPECT_NE(cipher, plain);
  EXPECT_EQ(crypt.Decrypt(cipher), plain);
}

TEST(AESCryptTest, RandomIvMakesCiphertextDifferent) {
  AESCrypt crypt(AESCrypt::GetKey("salt", "password"));
  std::string plain = "same plaintext";
  std::string c1 = crypt.Encrypt(plain);
  std::string c2 = crypt.Encrypt(plain);
  EXPECT_EQ(crypt.Decrypt(c1), plain);
  EXPECT_EQ(crypt.Decrypt(c2), plain);
  EXPECT_NE(c1, c2);
}

TEST(AESCryptTest, EmptyStringRoundtrip) {
  AESCrypt crypt(AESCrypt::GetKey("salt", "password"));
  std::string cipher = crypt.Encrypt("");
  EXPECT_FALSE(cipher.empty());
  EXPECT_EQ(crypt.Decrypt(cipher), "");
}

TEST(AESCryptTest, Utf8Roundtrip) {
  AESCrypt crypt(AESCrypt::GetKey("salt", "password"));
  std::string plain = "你好，埋点！{\"user\":\"张三\",\"score\":99.5}";
  EXPECT_EQ(crypt.Decrypt(crypt.Encrypt(plain)), plain);
}

TEST(AESCryptTest, BinaryDataRoundtrip) {
  AESCrypt crypt(AESCrypt::GetKey("salt", "password"));
  std::vector<char> raw = {'\0', '\x01', '\x80', '\xff', 'A', '\n', '\0'};
  std::string cipher =
      crypt.Encrypt(static_cast<const void*>(raw.data()), raw.size());
  std::string decrypted = crypt.Decrypt(cipher);
  std::vector<char> back(decrypted.begin(), decrypted.end());
  EXPECT_EQ(back, raw);
}

class BuriedDbTest : public ::testing::Test {
 protected:
  void SetUp() override {
    guard_ = std::make_unique<TempDirGuard>();
    db_ = std::make_unique<BuriedDb>(guard_->path() + "/test.db");
  }

  void TearDown() override {
    db_.reset();
    guard_.reset();
  }

  BuriedDb::Data MakeData(int32_t priority, const std::string& content) {
    BuriedDb::Data d;
    d.id = -1;
    d.priority = priority;
    d.timestamp = 1700000000000ULL + priority;
    d.content.assign(content.begin(), content.end());
    return d;
  }

  std::unique_ptr<TempDirGuard> guard_;
  std::unique_ptr<BuriedDb> db_;
};

TEST_F(BuriedDbTest, InsertAndQueryOrderByPriority) {
  db_->InsertData(MakeData(3, "event-A"));
  db_->InsertData(MakeData(1, "event-B"));
  db_->InsertData(MakeData(2, "event-C"));

  auto result = db_->QueryData(10);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result[0].priority, 3);
  EXPECT_EQ(result[1].priority, 2);
  EXPECT_EQ(result[2].priority, 1);

  EXPECT_NE(result[0].id, -1);
  EXPECT_NE(result[0].id, result[1].id);

  std::string content0(result[0].content.begin(), result[0].content.end());
  EXPECT_EQ(content0, "event-A");
}

TEST_F(BuriedDbTest, QueryLimit) {
  for (int i = 0; i < 5; ++i) {
    db_->InsertData(MakeData(i, "event-" + std::to_string(i)));
  }
  EXPECT_EQ(db_->QueryData(3).size(), 3u);
  EXPECT_EQ(db_->QueryData(100).size(), 5u);
}

TEST_F(BuriedDbTest, DeleteData) {
  db_->InsertData(MakeData(2, "A"));
  db_->InsertData(MakeData(1, "B"));
  auto all = db_->QueryData(10);
  ASSERT_EQ(all.size(), 2u);

  db_->DeleteData(all[0]);
  auto rest = db_->QueryData(10);
  ASSERT_EQ(rest.size(), 1u);
  std::string content(rest[0].content.begin(), rest[0].content.end());
  EXPECT_EQ(content, "B");
}

TEST_F(BuriedDbTest, DeleteDatas) {
  db_->InsertData(MakeData(2, "A"));
  db_->InsertData(MakeData(1, "B"));
  db_->InsertData(MakeData(0, "C"));
  auto all = db_->QueryData(10);
  ASSERT_EQ(all.size(), 3u);

  std::vector<BuriedDb::Data> to_delete = {all[0], all[1]};
  db_->DeleteDatas(to_delete);
  auto rest = db_->QueryData(10);
  ASSERT_EQ(rest.size(), 1u);
  std::string content(rest[0].content.begin(), rest[0].content.end());
  EXPECT_EQ(content, "C");
}

class BuriedApiTest : public ::testing::Test {
 protected:
  void SetUp() override { guard_ = std::make_unique<TempDirGuard>(); }
  void TearDown() override {
    buried_.reset();
    guard_.reset();
  }

  std::unique_ptr<TempDirGuard> guard_;
  std::unique_ptr<Buried> buried_;
};

TEST_F(BuriedApiTest, CreateInitWorkDirAndLogger) {
  buried_ = std::make_unique<Buried>(guard_->path());
  EXPECT_TRUE(PathExists(guard_->path() + "/buried"));
  EXPECT_TRUE(PathExists(guard_->path() + "/buried/buried.log"));
}

TEST_F(BuriedApiTest, StartAndReportReturnOk) {
  buried_ = std::make_unique<Buried>(guard_->path());
  EXPECT_EQ(buried_->Start(MakeConfig()), BuriedResult::kBuriedOK);
  EXPECT_EQ(buried_->Report("app_launch", "{\"page\":\"home\"}", 0),
            BuriedResult::kBuriedOK);
}

TEST_F(BuriedApiTest, ReportBeforeStartReturnsUnknown) {
  buried_ = std::make_unique<Buried>(guard_->path());
  EXPECT_EQ(buried_->Report("app_launch", "{}", 0),
            BuriedResult::kBuriedUnknown);
}

TEST(BuriedCApiTest, CreateWithNullWorkDir) {
  EXPECT_EQ(Buried_Create(nullptr), nullptr);
}

TEST(BuriedCApiTest, DestroyNullIsSafe) {
  Buried_Destroy(nullptr);
}

TEST(BuriedCApiTest, InvalidParams) {
  BuriedConfig config{};
  EXPECT_EQ(Buried_Start(nullptr, &config), BuriedResult::kBuriedInvalidParam);
  EXPECT_EQ(Buried_Report(nullptr, "t", "{}", 0),
            BuriedResult::kBuriedInvalidParam);
}

TEST(BuriedCApiTest, NullTitleOrDataRejected) {
  TempDirGuard guard;
  Buried* buried = Buried_Create(guard.path().c_str());
  ASSERT_NE(buried, nullptr);

  EXPECT_EQ(Buried_Report(buried, nullptr, "{}", 0),
            BuriedResult::kBuriedInvalidParam);
  EXPECT_EQ(Buried_Report(buried, "t", nullptr, 0),
            BuriedResult::kBuriedInvalidParam);

  Buried_Destroy(buried);
}

TEST(BuriedCApiTest, FullFlow) {
  TempDirGuard guard;

  Buried* buried = Buried_Create(guard.path().c_str());
  ASSERT_NE(buried, nullptr);

  BuriedConfig config{};
  config.host = "127.0.0.1";
  config.port = "80";
  config.topic = "/api/v1/report";
  config.user_id = "user_test";
  config.app_version = "1.0.0";
  config.app_name = "BuriedTest";
  config.custom_data = "{\"channel\":\"unittest\"}";
  config.report_batch_size = 5;
  config.use_https = 0;
  EXPECT_EQ(Buried_Start(buried, &config), BuriedResult::kBuriedOK);

  EXPECT_EQ(Buried_Report(buried, "app_launch", "{\"page\":\"home\"}", 0),
            BuriedResult::kBuriedOK);
  EXPECT_EQ(Buried_Report(buried, "button_click",
                          "{\"button\":\"submit\"}", 1),
            BuriedResult::kBuriedOK);

  Buried_Destroy(buried);
  SUCCEED();
}

}  // namespace
