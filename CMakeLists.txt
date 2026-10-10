cmake_minimum_required(VERSION 3.20)

project(CPUCpp LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
)

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

add_executable(test_fsm_control_unit
    tests/multi_cycle/test_fsm_control_unit.cpp
    src/InstructionDecoder.cpp
)

target_link_libraries(test_fsm_control_unit
    PRIVATE
        logic::logic
)

add_test(NAME test_fsm_control_unit COMMAND test_fsm_control_unit)

add_executable(test_pipelined_registers
    tests/pipelined/test_pipelined_registers.cpp
)

target_link_libraries(test_pipelined_registers
    PRIVATE
        logic::logic
)

add_test(NAME test_pipelined_registers COMMAND test_pipelined_registers)

add_executable(test_pipelined_muxes
    tests/pipelined/test_pipelined_muxes.cpp
)

target_link_libraries(test_pipelined_muxes
    PRIVATE
        logic::logic
)

add_test(NAME test_pipelined_muxes COMMAND test_pipelined_muxes)

add_executable(test_pipelined_datapath
    tests/pipelined/test_pipelined_datapath.cpp
    src/InstructionDecoder.cpp
)

target_link_libraries(test_pipelined_datapath
    PRIVATE
        logic::logic
)

add_test(NAME test_pipelined_datapath COMMAND test_pipelined_datapath)

add_executable(test_cpu_wrappers
    tests/test_cpu_wrappers.cpp
    src/InstructionDecoder.cpp
)

target_link_libraries(test_cpu_wrappers
    PRIVATE
        logic::logic
)

add_test(NAME test_cpu_wrappers COMMAND test_cpu_wrappers)

add_executable(test_cache_line
    tests/superscalar/test_cache_line.cpp
)

target_link_libraries(test_cache_line
    PRIVATE
        logic::logic
)

add_test(NAME test_cache_line COMMAND test_cache_line)

add_executable(test_memory_refill_integration
    tests/superscalar/test_memory_refill_integration.cpp
)

target_link_libraries(test_memory_refill_integration
    PRIVATE
        logic::logic
)

add_test(NAME test_memory_refill_integration COMMAND test_memory_refill_integration)

add_executable(test_fetch_unit
    tests/superscalar/test_fetch_unit.cpp
)

target_link_libraries(test_fetch_unit
    PRIVATE
        logic::logic
)

add_test(NAME test_fetch_unit COMMAND test_fetch_unit)

add_executable(test_fetch_icache_integration
    tests/superscalar/test_fetch_icache_integration.cpp
)

target_link_libraries(test_fetch_icache_integration
    PRIVATE
        logic::logic
)

add_test(NAME test_fetch_icache_integration COMMAND test_fetch_icache_integration)

add_executable(FetchDecodeRegisterTests
    tests/superscalar/FetchDecodeRegisterTests.cpp
)

target_link_libraries(FetchDecodeRegisterTests
    PRIVATE
        logic::logic
)

add_test(NAME FetchDecodeRegisterTests COMMAND FetchDecodeRegisterTests)

add_executable(DecodeUnitTests
    tests/superscalar/DecodeUnitTests.cpp
)

target_link_libraries(DecodeUnitTests
    PRIVATE
        logic::logic
)

add_test(NAME DecodeUnitTests COMMAND DecodeUnitTests)

add_executable(RenameUnitTests
    tests/superscalar/RenameUnitTests.cpp
)

target_link_libraries(RenameUnitTests
    PRIVATE
        logic::logic
)

add_test(NAME RenameUnitTests COMMAND RenameUnitTests)

add_executable(DispatchUnitTests
    tests/superscalar/DispatchUnitTests.cpp
)

target_link_libraries(DispatchUnitTests
    PRIVATE
        logic::logic
)

add_test(NAME DispatchUnitTests COMMAND DispatchUnitTests)

add_executable(IssueROBTests
    tests/superscalar/IssueROBTests.cpp
)

target_link_libraries(IssueROBTests
    PRIVATE
        logic::logic
)

add_test(NAME IssueROBTests COMMAND IssueROBTests)

add_executable(IssueQueueTests
    tests/superscalar/IssueQueueTests.cpp
)

target_link_libraries(IssueQueueTests
    PRIVATE
        logic::logic
)

add_test(NAME IssueQueueTests COMMAND IssueQueueTests)

add_executable(ALUExecutionUnitTests
    tests/superscalar/ALUExecutionUnitTests.cpp
)

target_link_libraries(ALUExecutionUnitTests
    PRIVATE
        logic::logic
)

add_test(NAME ALUExecutionUnitTests COMMAND ALUExecutionUnitTests)

add_executable(IntegerExecutionPipelineTests
    tests/superscalar/IntegerExecutionPipelineTests.cpp
)

target_link_libraries(IntegerExecutionPipelineTests
    PRIVATE
        logic::logic
)

add_test(NAME IntegerExecutionPipelineTests COMMAND IntegerExecutionPipelineTests)

add_executable(BranchExecutionUnitTests
    tests/superscalar/BranchExecutionUnitTests.cpp
)

target_link_libraries(BranchExecutionUnitTests
    PRIVATE
        logic::logic
)

add_test(NAME BranchExecutionUnitTests COMMAND BranchExecutionUnitTests)




# Register Alias Table tests
add_executable(test_register_alias_table
    tests/superscalar/RegisterAliasTableTests.cpp
)

target_link_libraries(test_register_alias_table
    PRIVATE
        logic::logic
)

add_test(NAME test_register_alias_table COMMAND test_register_alias_table)

# Physical Register Free List Recovery tests
add_executable(PhysicalRegisterFreeListRecoveryTests
    tests/superscalar/PhysicalRegisterFreeListRecoveryTests.cpp
)

target_link_libraries(PhysicalRegisterFreeListRecoveryTests
    PRIVATE
        logic::logic
)

add_test(NAME PhysicalRegisterFreeListRecoveryTests COMMAND PhysicalRegisterFreeListRecoveryTests)

# Branch Recovery Unit tests
add_executable(BranchRecoveryUnitTests
    tests/superscalar/BranchRecoveryUnitTests.cpp
)

target_link_libraries(BranchRecoveryUnitTests
    PRIVATE
        logic::logic
)

add_test(NAME BranchRecoveryUnitTests COMMAND BranchRecoveryUnitTests)

# Branch Prediction Unit tests
add_executable(BranchPredictionUnitTests
    tests/superscalar/BranchPredictionUnitTests.cpp
)

target_link_libraries(BranchPredictionUnitTests
    PRIVATE
        logic::logic
)

add_test(NAME BranchPredictionUnitTests COMMAND BranchPredictionUnitTests)

