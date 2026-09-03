cmake_minimum_required(VERSION 3.20)

project(CPUCpp LANGUAGES CXX)

enable_testing()

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

add_executable(test_instruction_memory
    tests/test_instruction_memory.cpp
)

target_link_libraries(test_instruction_memory
    PRIVATE
        logic::logic
)

add_test(NAME test_instruction_memory COMMAND test_instruction_memory)

add_executable(test_instruction_decoder
    tests/test_instruction_decoder.cpp
    src/InstructionDecoder.cpp
)

target_link_libraries(test_instruction_decoder
    PRIVATE
        logic::logic
)

add_executable(test_pc_imem_connection
    tests/test_pc_imem_connection.cpp
)

target_link_libraries(test_pc_imem_connection
    PRIVATE
        logic::logic
)

add_executable(test_control_unit
    tests/single_cycle/test_control_unit.cpp
)

target_link_libraries(test_control_unit
    PRIVATE
        logic::logic
)

add_executable(test_alu_interface
    tests/test_alu_interface.cpp
)

target_link_libraries(test_alu_interface
    PRIVATE
        logic::logic
)

add_executable(test_alu_operand_mux
    tests/test_alu_operand_mux.cpp
)

target_link_libraries(test_alu_operand_mux
    PRIVATE
        logic::logic
)

add_test(NAME test_alu_operand_mux COMMAND test_alu_operand_mux)

add_executable(test_single_cycle_datapath
    tests/single_cycle/test_single_cycle_datapath.cpp
    src/InstructionDecoder.cpp
)

target_link_libraries(test_single_cycle_datapath
    PRIVATE
        logic::logic
)

add_test(NAME test_single_cycle_datapath COMMAND test_single_cycle_datapath)

add_executable(test_data_memory
    tests/test_data_memory.cpp
)

target_link_libraries(test_data_memory
    PRIVATE
        logic::logic
)

add_test(NAME test_data_memory COMMAND test_data_memory)

add_executable(test_writeback_mux
    tests/test_writeback_mux.cpp
)

target_link_libraries(test_writeback_mux
    PRIVATE
        logic::logic
)

add_test(NAME test_writeback_mux COMMAND test_writeback_mux)

add_executable(test_multi_cycle_registers
    tests/multi_cycle/test_multi_cycle_registers.cpp
)

target_link_libraries(test_multi_cycle_registers
    PRIVATE
        logic::logic
)

add_test(NAME test_multi_cycle_registers COMMAND test_multi_cycle_registers)

add_executable(test_multi_cycle_muxes
    tests/multi_cycle/test_multi_cycle_muxes.cpp
)

target_link_libraries(test_multi_cycle_muxes
    PRIVATE
        logic::logic
)

add_test(NAME test_multi_cycle_muxes COMMAND test_multi_cycle_muxes)

add_executable(test_multi_cycle_datapath
    tests/multi_cycle/test_multi_cycle_datapath.cpp
    src/InstructionDecoder.cpp
)

target_link_libraries(test_multi_cycle_datapath
    PRIVATE
        logic::logic
)

add_test(NAME test_multi_cycle_datapath COMMAND test_multi_cycle_datapath)
