if(WIN32)
    set(STV_PLATFORM_SOURCES
        src/platform/path_service_windows.cpp
    )
else()
    set(STV_PLATFORM_SOURCES
        src/platform/path_service_unix.cpp
    )
endif()

add_library(stv_infra STATIC
    src/logger.cpp
    src/http_client.cpp
    src/curl_http_client.cpp
    src/api_client.cpp
    src/sse_client.cpp
    src/token_storage.cpp
    ${STV_PLATFORM_SOURCES}
)

target_include_directories(stv_infra
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(CURL REQUIRED)

target_link_libraries(stv_infra
    PUBLIC
        stv_core
        CURL::libcurl
        spdlog::spdlog
)

if(WIN32)
    target_link_libraries(stv_infra PRIVATE shell32 ole32)
endif()

target_compile_features(stv_infra PUBLIC cxx_std_17)
set_project_warnings(stv_infra)
