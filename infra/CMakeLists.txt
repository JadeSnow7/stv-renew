# ---- infra module: stv_infra ----
add_library(stv_infra STATIC
    src/logger.cpp
)

target_include_directories(stv_infra
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(stv_infra
    PUBLIC
        spdlog::spdlog
)

target_compile_features(stv_infra PUBLIC cxx_std_17)
