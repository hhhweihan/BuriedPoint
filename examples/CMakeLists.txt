# 示例程序构建配置
# 依赖根目录 CMakeLists.txt 中开启 BUILD_BURIED_EXAMPLES=ON

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/third_party
    ${PROJECT_SOURCE_DIR}/src/third_party/spdlog/include
    ${PROJECT_SOURCE_DIR}/src/third_party/boost
    ${PROJECT_SOURCE_DIR}/src/third_party/mbedtls/include
    ${CMAKE_BINARY_DIR}
)

# 示例1：C API 完整流程（创建 -> 配置 -> 启动 -> 上报 -> 销毁）
add_executable(buried_example buried_example.cc)
target_link_libraries(buried_example PRIVATE
    Buried_static
    spdlog
    mbedtls_static
    Threads::Threads
)

# 示例2：C++ 组件演示（加解密 / 数据库 / 设备信息）
add_executable(components_example components_example.cc)
target_link_libraries(components_example PRIVATE
    Buried_static
    spdlog
    mbedtls_static
    Threads::Threads
)
