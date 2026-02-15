# ---- infra module: stv_infra ----
add_library(stv_infra STATIC
    src/logger.cpp
    src/http_client.cpp
    src/curl_http_client.cpp
)

target_include_directories(stv_infra
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find libcurl
find_package(CURL REQUIRED)

target_link_libraries(stv_infra
    PUBLIC
        stv_core
        CURL::libcurl
)

# spdlog is optional (INTERFACE target if disabled)
if(NOT TARGET spdlog)
    message(WARNING "spdlog target not found, logger.cpp will use simple fallback")
endif()
target_link_libraries(stv_infra PUBLIC spdlog)

target_compile_features(stv_infra PUBLIC cxx_std_17)
