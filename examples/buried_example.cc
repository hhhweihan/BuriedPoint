// 埋点 SDK C API 使用示例
//
// 演示完整流程：
//   1. Buried_Create  创建埋点实例（指定工作目录，日志/数据库存放于此）
//   2. Buried_Start   组装配置并启动上报能力（支持批量上限与 HTTPS）
//   3. Buried_Report  上报埋点事件（后台会加密 -> 落库 -> 定时 HTTP(S) 上报）
//   4. Buried_Destroy 销毁实例
//
// 用法：
//   ./buried_example [http|https] [port] [batch_size]
//   默认: http 8080 10
//
// 配合测试服务端（server/report_server.py）可看到服务端按 report_id 去重后
// 收到的完整事件。上报失败时数据会保留在本地库中，下个周期以相同的
// report_id 重试，服务端据此幂等去重。

#include "include/buried.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <unistd.h>

int main(int argc, char* argv[]) {
  std::string mode = argc > 1 ? argv[1] : "http";
  std::string port = argc > 2 ? argv[2] : "8080";
  int batch_size = argc > 3 ? std::atoi(argv[3]) : 10;
  bool use_https = (mode == "https");

  // 1. 创建埋点实例（按进程号隔离工作目录，避免多次运行数据累积）
  std::string work_dir = "/tmp/buried_example_" + std::to_string(getpid());
  Buried* buried = Buried_Create(work_dir.c_str());
  if (buried == nullptr) {
    std::printf("[example] Buried_Create failed\n");
    return 1;
  }
  std::printf("[example] Buried_Create ok, work_dir=%s\n", work_dir.c_str());

  // 2. 组装配置并启动
  BuriedConfig config{};  // 必须零初始化，未设置的字段取默认值
  config.host = "127.0.0.1";
  config.port = port.c_str();
  config.topic = "/api/v1/report";
  config.user_id = "user_10086";
  config.app_version = "1.0.0";
  config.app_name = "BuriedPointExample";
  config.custom_data = "{\"channel\":\"AppStore\",\"lang\":\"zh-CN\"}";
  config.report_batch_size = static_cast<uint32_t>(batch_size);  // 每批最多 N 条
  config.use_https = use_https ? 1 : 0;                          // 1=HTTPS

  int32_t ret = Buried_Start(buried, &config);
  if (ret != 0) {
    std::printf("[example] Buried_Start failed, ret=%d\n", ret);
    Buried_Destroy(buried);
    return 1;
  }
  std::printf("[example] Buried_Start ok, %s://%s:%s%s, batch_size=%d\n",
              use_https ? "https" : "http", config.host, config.port,
              config.topic, batch_size);

  // 3. 上报 batch_size 条事件（title: 事件名, data: JSON 字符串, priority: 优先级）
  for (int i = 0; i < batch_size; ++i) {
    std::string data = "{\"index\":" + std::to_string(i) +
                       ",\"page\":\"home\",\"button\":\"submit\"}";
    Buried_Report(buried, "event_click", data.c_str(),
                  static_cast<uint32_t>(i % 3));
  }
  std::printf("[example] reported %d events\n", batch_size);

  // 4. 等待后台定时器触发上报（默认 5 秒一个周期），实际项目无需 sleep
  std::printf("[example] waiting for periodic report...\n");
  std::this_thread::sleep_for(std::chrono::seconds(6));

  // 5. 销毁实例
  Buried_Destroy(buried);
  std::printf("[example] done\n");
  return 0;
}
