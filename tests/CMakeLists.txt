# ---- tests ----
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
# Prevent gtest from overriding our compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Test: Task State Machine
add_executable(test_task_state_machine
    test_task_state_machine.cpp
)
target_link_libraries(test_task_state_machine
    PRIVATE stv_core GTest::gtest_main
)
gtest_discover_tests(test_task_state_machine)

# Test: Scheduler
add_executable(test_scheduler
    test_scheduler.cpp
)
target_link_libraries(test_scheduler
    PRIVATE stv_core GTest::gtest_main
)
gtest_discover_tests(test_scheduler)

# Test: Pipeline
add_executable(test_pipeline
    test_pipeline.cpp
)
target_link_libraries(test_pipeline
    PRIVATE stv_core GTest::gtest_main
)
gtest_discover_tests(test_pipeline)

# Test: HTTP Client
add_executable(test_http_client
    http_client_test.cpp
)
target_link_libraries(test_http_client
    PRIVATE stv_core stv_infra GTest::gtest_main
)
gtest_discover_tests(test_http_client)
