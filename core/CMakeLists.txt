add_library(stv_core STATIC
    src/task.cpp
    src/cancel_token.cpp
    src/scheduler.cpp
    src/pipeline.cpp
    src/orchestrator.cpp
)

target_include_directories(stv_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/infra/include
)

# Avoid circular link with stv_infra.
# core only needs infra headers (ILogger interface) at compile time.
target_compile_features(stv_core PUBLIC cxx_std_17)
set_project_warnings(stv_core)
