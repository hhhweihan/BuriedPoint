include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/third_party
    ${PROJECT_SOURCE_DIR}/src/third_party/spdlog/include
    ${PROJECT_SOURCE_DIR}/src/third_party/boost
    ${PROJECT_SOURCE_DIR}/src/third_party/mbedtls/include
    ${CMAKE_BINARY_DIR}
)

add_executable(buried_tests buried_test.cc)
target_link_libraries(buried_tests PRIVATE
    Buried_static
    spdlog
    mbedtls_static
    gtest
    gtest_main
    Threads::Threads
)

enable_testing()
add_test(NAME buried_tests COMMAND buried_tests)
