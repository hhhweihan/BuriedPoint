// C++ 组件独立使用示例
//
// 演示埋点 SDK 内部各组件的能力：
//   1. CommonService 设备信息采集（系统版本/设备名/设备ID等）
//   2. AESCrypt       AES-256-CBC 加解密（密钥由 PBKDF2 派生）
//   3. BuriedDb       SQLite 本地存储（按优先级排序查询/删除）
//
// 编译运行：
//   cmake -B build -DBUILD_BURIED_EXAMPLES=ON
//   cmake --build build
//   ./build/examples/components_example

#include "common/common_service.h"
#include "crypt/crypt.h"
#include "database/database.h"
#include "fs_util.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace buried;

int main() {
  // ========== 1. CommonService：设备信息 ==========
  std::printf("===== CommonService 设备信息 =====\n");
  CommonService service;
  std::printf("system_version : %s\n", service.system_version.c_str());
  std::printf("device_name    : %s\n", service.device_name.c_str());
  std::printf("device_id      : %s\n", service.device_id.c_str());
  std::printf("buried_version : %s\n", service.buried_version.c_str());
  std::printf("lifecycle_id   : %s\n", service.lifecycle_id.c_str());
  std::printf("now_date       : %s\n", CommonService::GetNowDate().c_str());
  std::printf("process_time   : %s\n", CommonService::GetProcessTime().c_str());
  std::printf("random_id      : %s\n", CommonService::GetRandomId().c_str());

  // ========== 2. AESCrypt：加解密 ==========
  std::printf("\n===== AESCrypt 加解密 =====\n");
  std::string key = AESCrypt::GetKey("buried_salt", "buried_password");
  AESCrypt crypt(key);

  std::string plain = "{\"event\":\"app_launch\",\"user\":\"张三\"}";
  std::string cipher = crypt.Encrypt(plain);
  std::string decrypted = crypt.Decrypt(cipher);
  std::printf("plain   : %s\n", plain.c_str());
  std::printf("cipher  : %zu bytes (hex 前16字节)\n", cipher.size());
  for (size_t i = 0; i < cipher.size() && i < 16; ++i) {
    std::printf("%02x ", static_cast<unsigned char>(cipher[i]));
  }
  std::printf("\n");
  std::printf("decrypt : %s\n", decrypted.c_str());
  std::printf("roundtrip %s\n", (decrypted == plain) ? "OK" : "FAILED");

  // ========== 3. BuriedDb：本地存储 ==========
  std::printf("\n===== BuriedDb 本地存储 =====\n");
  std::string db_dir = "/tmp/buried_components_example";
  CreateDirectories(db_dir);
  {
    BuriedDb db(db_dir + "/demo.db");

    std::vector<BuriedDb::Data> datas;
    auto make_data = [](int32_t priority, const char* text) {
      BuriedDb::Data d;
      d.id = -1;  // 自增主键，插入时忽略
      d.priority = priority;
      d.timestamp = 1700000000000ULL + priority * 1000;
      d.content.assign(text, text + std::char_traits<char>::length(text));
      return d;
    };
    datas.push_back(make_data(3, "event-A"));
    datas.push_back(make_data(1, "event-B"));
    datas.push_back(make_data(2, "event-C"));
    for (const auto& d : datas) {
      db.InsertData(d);
    }
    std::printf("inserted 3 rows\n");

    auto result = db.QueryData(10);
    std::printf("query all, size=%zu (按 priority 降序):\n", result.size());
    for (const auto& r : result) {
      std::string content(r.content.begin(), r.content.end());
      std::printf("  id=%d priority=%d content=%s\n", r.id, r.priority,
                  content.c_str());
    }

    if (!result.empty()) {
      db.DeleteData(result.front());
      std::printf("after delete one, size=%zu\n", db.QueryData(10).size());
    }
  }
  return 0;
}
