cmake_minimum_required(VERSION 3.20)

project(CPUCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory("../Logic.cpp" "${CMAKE_BINARY_DIR}/Logic.cpp-build")

add_executable(cpu_cpp
    src/main.cpp
)

target_link_libraries(cpu_cpp
    PRIVATE
        logic::logic
)
