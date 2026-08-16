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

add_executable(test_instruction
    tests/test_instruction.cpp
)

target_link_libraries(test_instruction
    PRIVATE
        logic::logic
)

add_test(NAME test_instruction COMMAND test_instruction)

add_executable(test_pc
    tests/test_pc.cpp
)

target_link_libraries(test_pc
    PRIVATE
        logic::logic
)

add_test(NAME test_pc COMMAND test_pc)


