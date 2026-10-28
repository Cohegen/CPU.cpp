cmake_minimum_required(VERSION 3.20)

project(CPUCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Logic.cpp/CMakeLists.txt")
    add_subdirectory("../Logic.cpp" "${CMAKE_BINARY_DIR}/Logic.cpp-build")
else()
    include(FetchContent)
    FetchContent_Declare(
        LogicCpp
        GIT_REPOSITORY https://github.com/Cohegen/Logic.cpp.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(LogicCpp)
endif()

# ------------------------------------------------------------------------------
# Include Directories
# ------------------------------------------------------------------------------
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/decode"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/fetch"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/memory"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/rename"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/dispatch"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/issue"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/execution"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/branch"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/commit"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/memory/instruction"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/core/memory/data"
    "${CMAKE_CURRENT_SOURCE_DIR}/superscalar/datapath"
    "${CMAKE_CURRENT_SOURCE_DIR}/vector_cpu/core"
)

# ------------------------------------------------------------------------------
# Main Application
# ------------------------------------------------------------------------------
add_executable(cpu_cpp src/main.cpp)
target_link_libraries(cpu_cpp PRIVATE logic::logic)

# ------------------------------------------------------------------------------
# Tests (Optional)
# ------------------------------------------------------------------------------
option(BUILD_TESTS "Build test suite" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
