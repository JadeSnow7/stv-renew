add_library(stv_core STATIC
    src/task.cpp
    src/cancel_token.cpp
    src/scheduler.cpp
    src/thread_pool_scheduler.cpp
    src/pipeline.cpp
    src/orchestrator.cpp
)

target_include_directories(stv_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# core stays infra-independent (pure C++ orchestration/scheduling).
target_compile_features(stv_core PUBLIC cxx_std_17)
set_project_warnings(stv_core)
