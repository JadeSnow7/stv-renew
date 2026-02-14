cmake_minimum_required(VERSION 3.21)
project(StoryToVideoRenew
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "StoryToVideo Renew — AI Video Generation Desktop Client"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Global warning flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
endif()

# ---- Options ----
option(STV_BUILD_TESTS "Build unit tests" ON)
option(STV_BUILD_APP   "Build Qt application (requires Qt6)" ON)

# ---- Third-party dependencies ----
# spdlog via FetchContent
include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# ---- Sub-projects (dependency order: infra → core → app) ----
add_subdirectory(infra)
add_subdirectory(core)

if(STV_BUILD_APP)
    add_subdirectory(app)
endif()

if(STV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
