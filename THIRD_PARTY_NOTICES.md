# Third-Party Notices

本仓库（BuriedPoint）使用以下第三方开源软件。各自许可证的完整文本见对应目录内的许可文件。

| 组件 | 版本 | 许可证 | 许可文件位置 | 用途 |
|---|---|---|---|---|
| spdlog | 1.12.0 | MIT | `src/third_party/spdlog/LICENSE` | 日志 |
| mbedtls | 3.5.0 | Apache-2.0 OR GPL-2.0-or-later（双许可，本项目按 Apache-2.0 使用） | `src/third_party/mbedtls/LICENSE` | AES 加解密、TLS（HTTPS 上报） |
| SQLite | - | Public Domain | `src/third_party/sqlite/PUBLIC_DOMAIN.txt` | 本地事件存储（sqlite3 C API 直连） |
| Boost | 1.83.0 | Boost Software License 1.0 | `src/third_party/boost/LICENSE_1_0.txt` | 异步 IO（仅保留 asio/system/date_time 等用到的模块） |
| nlohmann/json | 3.x | MIT | `src/third_party/nlohmann/LICENSE` | JSON 解析/序列化 |
| googletest | latest | BSD-3-Clause | `googletest/LICENSE` | 单元测试 |
