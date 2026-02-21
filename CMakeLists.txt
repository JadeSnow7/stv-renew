cmake_minimum_required(VERSION 3.21)
project(
    StoryToVideoRenew
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "StoryToVideo Renew - AI Video Generation Desktop Client"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STV_BUILD_TESTS "Build unit tests" ON)
option(STV_BUILD_APP "Build Qt application (requires Qt6)" ON)
option(STV_USE_SYSTEM_SPDLOG "Use system-installed spdlog package" ON)

# Locked decision: spdlog is mandatory across platforms.
if(STV_USE_SYSTEM_SPDLOG)
    find_package(spdlog QUIET)
endif()

if(NOT TARGET spdlog::spdlog)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

if(NOT TARGET spdlog::spdlog)
    message(FATAL_ERROR "spdlog::spdlog target is required")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)

# ---- Sub-projects (dependency order: infra_base + core → infra_net → app) ----
add_subdirectory(core)
add_subdirectory(infra)

if(STV_BUILD_APP)
    add_subdirectory(app)
endif()

if(STV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
