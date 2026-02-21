# ---- tests ----
option(STV_ENABLE_NETWORK_TESTS "Enable tests that require network access" OFF)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(test_task_state_machine
    test_task_state_machine.cpp
)
target_link_libraries(test_task_state_machine PRIVATE stv_core GTest::gtest_main)
set_project_warnings(test_task_state_machine)
gtest_discover_tests(test_task_state_machine)

add_executable(test_scheduler
    test_scheduler.cpp
)
target_link_libraries(test_scheduler PRIVATE stv_core GTest::gtest_main)
set_project_warnings(test_scheduler)
gtest_discover_tests(test_scheduler)

add_executable(test_thread_pool_scheduler
    test_thread_pool_scheduler.cpp
)
target_link_libraries(test_thread_pool_scheduler PRIVATE stv_core GTest::gtest_main)
set_project_warnings(test_thread_pool_scheduler)
gtest_discover_tests(test_thread_pool_scheduler)

add_executable(test_pipeline
    test_pipeline.cpp
)
target_link_libraries(test_pipeline PRIVATE stv_core GTest::gtest_main)
set_project_warnings(test_pipeline)
gtest_discover_tests(test_pipeline)

add_executable(test_orchestrator
    test_orchestrator.cpp
)
target_link_libraries(test_orchestrator PRIVATE stv_core GTest::gtest_main)
set_project_warnings(test_orchestrator)
gtest_discover_tests(test_orchestrator)

add_executable(test_http_client
    http_client_test.cpp
)
target_link_libraries(test_http_client PRIVATE stv_core stv_infra GTest::gtest_main)
set_project_warnings(test_http_client)
gtest_discover_tests(test_http_client)

# Test: CURL HTTP Client (integration test, local loopback fixture)
add_executable(test_curl_http_client
    curl_http_client_test.cpp
)
target_link_libraries(test_curl_http_client
    PRIVATE stv_core stv_infra GTest::gtest_main
)
message(STATUS "curl_http_client_test uses local loopback fixture (no external network required)")
gtest_discover_tests(test_curl_http_client)

add_executable(test_path_service
    test_path_service.cpp
)
target_link_libraries(test_path_service PRIVATE stv_infra GTest::gtest_main)
set_project_warnings(test_path_service)
gtest_discover_tests(test_path_service)

add_executable(test_token_storage
    test_token_storage.cpp
)
target_link_libraries(test_token_storage PRIVATE stv_infra GTest::gtest_main)
set_project_warnings(test_token_storage)
gtest_discover_tests(test_token_storage)
