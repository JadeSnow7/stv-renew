if(WIN32)
    set(STV_PLATFORM_SOURCES
        src/platform/path_service_windows.cpp
    )
else()
    set(STV_PLATFORM_SOURCES
        src/platform/path_service_unix.cpp
    )
endif()

# ---- infra_base: logger and basic utilities (no core dependency) ----
add_library(stv_infra_base STATIC
    src/logger.cpp
)

target_include_directories(stv_infra_base
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/core/include
)

target_compile_features(stv_infra_base PUBLIC cxx_std_17)
target_link_libraries(stv_infra_base PUBLIC spdlog::spdlog)
set_project_warnings(stv_infra_base)

# ---- infra_net: HTTP client and platform services (depends on core) ----
add_library(stv_infra_net STATIC
    src/http_client.cpp
    src/stages.cpp
    src/api_client.cpp
    src/sse_client.cpp
    src/token_storage.cpp
    ${STV_PLATFORM_SOURCES}
)

target_include_directories(stv_infra_net
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find libcurl (optional so core/tests can still build on minimal machines)
find_package(CURL QUIET)
if(CURL_FOUND)
    target_sources(stv_infra_net PRIVATE src/curl_http_client.cpp)
    target_link_libraries(stv_infra_net PUBLIC CURL::libcurl)
    target_compile_definitions(stv_infra_net PUBLIC STV_HAS_CURL=1)
    message(STATUS "libcurl found: enabling CurlHttpClient")
else()
    target_sources(stv_infra_net PRIVATE src/curl_http_client_stub.cpp)
    target_compile_definitions(stv_infra_net PUBLIC STV_HAS_CURL=0)
    message(WARNING "libcurl not found: CurlHttpClient will use stub implementation")
endif()

target_link_libraries(stv_infra_net
    PUBLIC
        stv_core
        stv_infra_base
)

if(WIN32)
    target_link_libraries(stv_infra_net PRIVATE shell32 ole32)
endif()

target_compile_features(stv_infra_net PUBLIC cxx_std_17)
set_project_warnings(stv_infra_net)

# ---- Legacy alias for backward compatibility ----
add_library(stv_infra INTERFACE)
target_link_libraries(stv_infra INTERFACE stv_infra_base stv_infra_net)
target_compile_features(stv_infra INTERFACE cxx_std_17)
