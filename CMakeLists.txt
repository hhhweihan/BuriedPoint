cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME "BuriedPoint")
project(${PROJECT_NAME} VERSION 1.0.0 LANGUAGES C CXX)

option(BUILD_BURIED_SHARED_LIBS "build shared library" OFF)
option(BUILD_BURIED_STATIC_LIBS "build static library" ON)
option(BUILD_BURIED_EXAMPLES "build_examples" OFF)
option(BUILD_BURIED_TEST "build unittest" OFF)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

include_directories(${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR})

message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_definitions(-DDEBUG)
else()
    add_definitions(-DNDEBUG)
endif()

add_subdirectory(src)

if(BUILD_BURIED_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_BURIED_TEST)
    include_directories(googletest/googletest googletest/googletest/include)
    enable_testing()
    add_subdirectory(googletest)
    add_subdirectory(tests)
endif()