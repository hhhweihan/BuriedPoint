# BuriedPoint 埋点 SDK

一个轻量级、跨平台的客户端埋点（数据采集上报）SDK。业务侧通过极简的 C API 上报事件，SDK 内部完成 **数据加密 → 本地落库 → 定时批量上报** 的全流程，上报失败自动保留数据、下个周期以相同的 `report_id` 重试，服务端可据此幂等去重，不丢数据、不重复统计。

## 特性

- **C API 对外**：一个头文件即可接入，方便 C/C++/其他语言绑定使用
- **异步架构**：基于 Boost.Asio 的 `io_context` + `strand` 串行化主逻辑与上报逻辑，无锁设计；空闲时线程休眠，不空转
- **加密存储**：事件内容使用 AES-256-CBC 加密后写入 SQLite（密钥由 PBKDF2-HMAC-SHA256 派生），每次加密使用随机 IV（密文前置 16 字节 IV）
- **本地缓存与重试**：事件先落库再上报，失败保留、定时重试（默认 5 秒一个周期）
- **批量上限可配置**：每批上报条数通过 `report_batch_size` 配置（0 = 默认 10）
- **HTTP / HTTPS 双通道**：HTTPS 使用 mbedtls TLS 客户端（无需系统 OpenSSL），通过 `use_https` 开启
- **上报去重**：每条数据携带 `report_id`（重试间不变），服务端按 `report_id` 幂等去重
- **设备信息采集**：系统版本、设备名、设备 ID（持久化到 `~/.buried/device_id`）、进程启动时间、随机 ID 等（macOS/Linux 双平台实现）
- **单元测试与示例**：googletest 23 个用例 + 2 个示例 + 端到端集成测试脚本

## 目录结构

```
BuriedPoint/
├── include/
│   └── buried.h                 # 对外 C API（唯一需要引用的头文件）
├── src/
│   ├── buried.cc                # C API 实现
│   ├── buried_core.{h,cc}       # Buried 核心类：初始化、日志、入口
│   ├── buried_common.h          # 返回码定义
│   ├── buried_config.h.in       # 版本号模板（cmake 生成 buried_config.h）
│   ├── fs_util.h                # 路径工具
│   ├── common/                  # 设备信息采集（macOS/Linux 双平台）
│   ├── context/                 # 全局异步上下文（io_context + work_guard + 双线程）
│   ├── crypt/                   # AES-256-CBC 加解密（mbedtls）
│   ├── database/                # SQLite 存储（sqlite3 C API，按优先级排序）
│   └── report/                  # 上报逻辑 + HTTP(S) 客户端（Boost.Asio + mbedtls TLS）
├── examples/                    # 示例程序（含详细注释）
├── tests/                       # googletest 单元测试
├── server/                      # 测试服务端（Python）与集成测试脚本
├── googletest/                  # 第三方测试框架（vendored）
└── CMakeLists.txt
```

## 构建

依赖：CMake ≥ 3.20、支持 C++14 的编译器（macOS/Linux 已验证）。第三方库（spdlog / mbedtls / sqlite / boost / nlohmann-json）已全部 vendored，**无需联网安装**。

```bash
# 只构建核心静态库
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 构建 + 示例 + 单元测试（推荐）
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_BURIED_EXAMPLES=ON \
      -DBUILD_BURIED_TEST=ON
cmake --build build
```

产物：`build/src/libBuried_static.a`

## 使用

### 方式一：C API（推荐）

```c
#include "include/buried.h"

/* 1. 创建实例，指定工作目录（日志 buried.log、数据库 buried.db 存放于此） */
Buried* buried = Buried_Create("/tmp/my_app_data");

/* 2. 组装配置并启动 */
BuriedConfig config = {0};              /* 必须零初始化，未设置的字段取默认值 */
config.host              = "127.0.0.1";
config.port              = "8443";
config.topic             = "/api/v1/report";
config.user_id           = "user_10086";
config.app_version       = "1.0.0";
config.app_name          = "MyApp";
config.custom_data       = "{\"channel\":\"AppStore\"}";  /* JSON 字符串 */
config.report_batch_size = 20;          /* 每批最多 20 条，0 = 默认 10 */
config.use_https         = 1;           /* 1 = HTTPS，0 = HTTP */
Buried_Start(buried, &config);

/* 3. 上报事件：title=事件名, data=JSON 字符串, priority=优先级(越小越先上报) */
Buried_Report(buried, "app_launch",   "{\"page\":\"home\"}", 0);
Buried_Report(buried, "button_click", "{\"button\":\"submit\"}", 1);

/* 4. 退出前销毁 */
Buried_Destroy(buried);
```

### 方式二：C++ API

```cpp
#include "buried_core.h"

Buried::Config config;
config.host = "127.0.0.1";
config.port = "8443";
config.topic = "/api/v1/report";
config.user_id = "user_1";
config.app_version = "1.0.0";
config.app_name = "MyApp";
config.custom_data = "{\"channel\":\"AppStore\"}";
config.report_batch_size = 20;
config.use_https = true;

Buried buried("/tmp/my_app_data");
buried.Start(config);
buried.Report("app_launch", "{\"page\":\"home\"}", 0);
```

完整可运行代码见 `examples/buried_example.cc`（C API，支持参数化）与 `examples/components_example.cc`（内部组件演示）。

## 运行单元测试

```bash
ctest --test-dir build --output-on-failure
# 或直接运行
./build/tests/buried_tests
```

测试覆盖：路径工具、设备信息格式、AES 密钥派生/加解密往返（空串/中文/二进制/随机 IV）、SQLite 存取（优先级排序/限额/删除）、Buried 核心与 C API 全流程及参数校验。

## 端到端测试（需要 server）

`server/` 目录提供 Python 测试服务端，支持 HTTP/HTTPS 并按 `report_id` 幂等去重：

```bash
# 1. 生成 HTTPS 自签名证书（client 当前为 verify_none 模式，证书无需受信任）
./server/gen_cert.sh

# 2. 启动测试 server（可选端口/模式）
python3 -u server/report_server.py 8080 http          # HTTP
python3 -u server/report_server.py 8443 https         # HTTPS

# 3. 跑示例客户端（http|https 端口 批量上限）
./build/examples/buried_example http 8080 10
./build/examples/buried_example https 8443 5

# 4. 或一键跑集成测试（自动起 server、校验收发与去重）
bash server/test_report.sh
```

集成测试覆盖：HTTP 上报、`report_id` 幂等去重（模拟客户端重试）、HTTPS 上报（mbedtls TLS + 自签名证书）。

## 上报数据格式与去重语义

每条上报事件按如下 JSON 组装，整体 AES 加密后入库：

```json
{
  "title": "app_launch",
  "data": "{\"page\":\"home\"}",
  "user_id": "user_10086",
  "app_version": "1.0.0",
  "app_name": "MyApp",
  "custom_data": {"channel": "AppStore"},
  "system_version": "26.5.2",
  "device_name": "My-Mac.local",
  "device_id": "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
  "buried_version": "1.0.0",
  "lifecycle_id": "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy",
  "priority": 0,
  "timestamp": "2026-08-19 11:29:00",
  "process_time": "2026-08-19 11:28:59.594",
  "report_id": "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"
}
```

**去重语义**：`report_id` 在数据入库时生成并随数据持久化；若上报失败，数据保留在本地库，下个周期以**相同**的 `report_id` 重试。服务端只需以 `report_id` 为唯一键即可实现幂等去重（参考 `server/report_server.py` 的实现）。

定时上报时按优先级降序取出最多 `report_batch_size` 条（默认 10），解密后组装为 JSON 数组 POST 到 `http(s)://<host>:<port><topic>`，成功即从库中删除。

## 平台兼容性

- **macOS**：系统版本走 `sysctl`，进程启动时间走 `kinfo_proc`
- **Linux**：系统版本走 `uname`，进程启动时间解析 `/proc/self/stat` + `/proc/stat`
- 其他平台：系统版本/进程时间返回空字符串兜底，不影响主流程
- HTTPS 使用 mbedtls（vendored），不依赖系统 OpenSSL

## 已知限制 / 后续规划

- HTTPS 目前为 `verify_none` 模式（便于自签名证书测试），生产环境应配置证书校验（CA 加载）
- 上报为同步请求，暂无超时与重试退避，服务端需保证快速响应
- 查询默认仅按 `priority` 排序，同优先级内未按时间二次排序
- 密钥派生参数（盐/密码）目前为内置默认值，生产环境建议外部注入
- 计划：Windows 适配、证书校验配置、上报批量上限动态调整、请求超时控制
