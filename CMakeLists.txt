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
# Test Helper Function
# ------------------------------------------------------------------------------
function(add_cpu_test target source)
    add_executable(${target} ${source} ${ARGN})
    target_link_libraries(${target} PRIVATE logic::logic)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

# ------------------------------------------------------------------------------
# Core / Single-Cycle / Multi-Cycle / Pipelined Tests
# ------------------------------------------------------------------------------
add_cpu_test(test_instruction                   tests/test_instruction.cpp)
add_cpu_test(test_pc                            tests/test_pc.cpp)
add_cpu_test(test_instruction_memory            tests/test_instruction_memory.cpp)
add_cpu_test(test_instruction_decoder           tests/test_instruction_decoder.cpp src/InstructionDecoder.cpp)
add_cpu_test(test_pc_imem_connection           tests/test_pc_imem_connection.cpp)
add_cpu_test(test_control_unit                  tests/single_cycle/test_control_unit.cpp)
add_cpu_test(test_alu_interface                 tests/test_alu_interface.cpp)
add_cpu_test(test_alu_operand_mux               tests/test_alu_operand_mux.cpp)
add_cpu_test(test_single_cycle_datapath         tests/single_cycle/test_single_cycle_datapath.cpp src/InstructionDecoder.cpp)
add_cpu_test(test_data_memory                   tests/test_data_memory.cpp)
add_cpu_test(test_writeback_mux                 tests/test_writeback_mux.cpp)
add_cpu_test(test_multi_cycle_registers         tests/multi_cycle/test_multi_cycle_registers.cpp)
add_cpu_test(test_multi_cycle_muxes             tests/multi_cycle/test_multi_cycle_muxes.cpp)
add_cpu_test(test_multi_cycle_datapath          tests/multi_cycle/test_multi_cycle_datapath.cpp src/InstructionDecoder.cpp)
add_cpu_test(test_fsm_control_unit              tests/multi_cycle/test_fsm_control_unit.cpp src/InstructionDecoder.cpp)
add_cpu_test(test_pipelined_registers           tests/pipelined/test_pipelined_registers.cpp)
add_cpu_test(test_pipelined_muxes               tests/pipelined/test_pipelined_muxes.cpp)
add_cpu_test(test_pipelined_datapath            tests/pipelined/test_pipelined_datapath.cpp src/InstructionDecoder.cpp)
add_cpu_test(test_cpu_wrappers                  tests/test_cpu_wrappers.cpp src/InstructionDecoder.cpp)

# ------------------------------------------------------------------------------
# Superscalar Core Tests
# ------------------------------------------------------------------------------
add_cpu_test(test_cache_line                    tests/superscalar/test_cache_line.cpp)
add_cpu_test(test_memory_refill_integration     tests/superscalar/test_memory_refill_integration.cpp)
add_cpu_test(test_fetch_unit                    tests/superscalar/test_fetch_unit.cpp)
add_cpu_test(test_fetch_icache_integration      tests/superscalar/test_fetch_icache_integration.cpp)
add_cpu_test(FetchDecodeRegisterTests           tests/superscalar/FetchDecodeRegisterTests.cpp)
add_cpu_test(DecodeUnitTests                    tests/superscalar/DecodeUnitTests.cpp)
add_cpu_test(RenameUnitTests                    tests/superscalar/RenameUnitTests.cpp)
add_cpu_test(DispatchUnitTests                  tests/superscalar/DispatchUnitTests.cpp)
add_cpu_test(IssueROBTests                      tests/superscalar/IssueROBTests.cpp)
add_cpu_test(IssueQueueTests                    tests/superscalar/IssueQueueTests.cpp)
add_cpu_test(ALUExecutionUnitTests              tests/superscalar/ALUExecutionUnitTests.cpp)
add_cpu_test(IntegerExecutionPipelineTests      tests/superscalar/IntegerExecutionPipelineTests.cpp)
add_cpu_test(BranchExecutionUnitTests           tests/superscalar/BranchExecutionUnitTests.cpp)
add_cpu_test(test_register_alias_table          tests/superscalar/RegisterAliasTableTests.cpp)
add_cpu_test(PhysicalRegisterFreeListRecoveryTests tests/superscalar/PhysicalRegisterFreeListRecoveryTests.cpp)
add_cpu_test(BranchRecoveryUnitTests            tests/superscalar/BranchRecoveryUnitTests.cpp)
add_cpu_test(BranchPredictionUnitTests          tests/superscalar/BranchPredictionUnitTests.cpp)
add_cpu_test(BranchMispredictionIntegrationTests tests/superscalar/BranchMispredictionIntegrationTests.cpp)
add_cpu_test(CommitUnitTests                    tests/superscalar/CommitUnitTests.cpp)
add_cpu_test(LSQMemoryTests                     tests/superscalar/LSQMemoryTests.cpp)
add_cpu_test(SuperScalarCPUTests                tests/superscalar/SuperScalarCPUTests.cpp)
add_cpu_test(DataCacheTests                     tests/superscalar/DataCacheTests.cpp)
add_cpu_test(SuperScalarBenchmarkTests          tests/superscalar/SuperScalarBenchmarkTests.cpp)

# ------------------------------------------------------------------------------
# Vector CPU Tests
# ------------------------------------------------------------------------------
add_cpu_test(test_vector_register               tests/vector/test_vector_register.cpp)
add_cpu_test(test_vector_register_file          tests/vector/test_vector_register_file.cpp)
add_cpu_test(test_minimum_unit                  tests/vector/test_minimum_unit.cpp)
add_cpu_test(test_vlmax_unit                    tests/vector/test_vlmax_unit.cpp)
add_cpu_test(test_vector_configuration_unit     tests/vector/test_vector_configuration_unit.cpp)
add_cpu_test(test_vector_lane_mask_unit         tests/vector/test_vector_lane_mask_unit.cpp)
add_cpu_test(test_vector_masked_wb_unit         tests/vector/test_vector_masked_wb_unit.cpp)
add_cpu_test(test_vector_lane                   tests/vector/test_vector_lane.cpp)
add_cpu_test(test_vector_execution_unit         tests/vector/test_vector_execution_unit.cpp)
