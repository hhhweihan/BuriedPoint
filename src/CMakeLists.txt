include_guard(GLOBAL)

include_directories(. third_party third_party/spdlog/include third_party/boost third_party/mbedtls/include ${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DBOOST_JSON_NO_LIB)
add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/mbedtls/library)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/buried_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/buried_config.h)

set(BURIED_SRCS 
    buried.cc 
    buried_core.cc
    context/context.cc    
    common/common_service.cc
    database/database.cc
    third_party/sqlite/sqlite3.c
    crypt/crypt.cc
    report/buried_report.cc 
    report/http_report.cc 
)
set(MODULE_NAME "Buried")

if(BUILD_BURIED_STATIC_LIBS)
    add_library(${MODULE_NAME}_static STATIC ${BURIED_SRCS})
    target_link_libraries(${MODULE_NAME}_static PRIVATE Threads::Threads spdlog mbedtls_static)
endif()

if(BUILD_BURIED_SHARED_LIBS)
    add_library(${MODULE_NAME}_shared SHARED ${BURIED_SRCS})
    target_link_libraries(${MODULE_NAME}_shared PRIVATE Threads::Threads spdlog mbedtls_static)
endif()

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/sqlite3.c PROPERTIES COMPILE_FLAGS "-w")