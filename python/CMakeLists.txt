cmake_minimum_required(VERSION 3.20)
project(pycpu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find pybind11
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.6
    )
    FetchContent_MakeAvailable(pybind11)
endif()

# Locate Logic.cpp
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../Logic.cpp/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../Logic.cpp" "${CMAKE_BINARY_DIR}/Logic.cpp-build")
else()
    include(FetchContent)
    FetchContent_Declare(
        LogicCpp
        GIT_REPOSITORY https://github.com/Cohegen/Logic.cpp.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(LogicCpp)
endif()

set(CPU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

pybind11_add_module(_pycpu_core
    src/bindings.cpp
    ${CPU_ROOT}/src/InstructionDecoder.cpp
)

target_include_directories(_pycpu_core PRIVATE
    ${CPU_ROOT}
    ${CPU_ROOT}/include
    ${CPU_ROOT}/components
    ${CPU_ROOT}/single_cycle_cpu
    ${CPU_ROOT}/single_cycle_cpu/datapath
    ${CPU_ROOT}/pipelined_cpu
)

target_link_libraries(_pycpu_core PRIVATE
    logic::logic
)

if(MINGW)
    target_link_options(_pycpu_core PRIVATE -static-libgcc -static-libstdc++)
endif()

# Place compiled .pyd / .so directly into pycpu/ for immediate importing
set_target_properties(_pycpu_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pycpu"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pycpu"
)
