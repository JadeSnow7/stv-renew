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

# Locked decision: spdlog is mandatory across platforms.
find_package(spdlog REQUIRED)
if(NOT TARGET spdlog::spdlog)
    message(FATAL_ERROR "spdlog::spdlog target is required")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)

add_subdirectory(core)
add_subdirectory(infra)

if(STV_BUILD_APP)
    add_subdirectory(app)
endif()

if(STV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
