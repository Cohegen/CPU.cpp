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
add_cpu_test(test_instruction                   test_instruction.cpp)
add_cpu_test(test_pc                            test_pc.cpp)
add_cpu_test(test_instruction_memory            test_instruction_memory.cpp)
add_cpu_test(test_instruction_decoder           test_instruction_decoder.cpp ${PROJECT_SOURCE_DIR}/src/InstructionDecoder.cpp)
add_cpu_test(test_pc_imem_connection           test_pc_imem_connection.cpp)
add_cpu_test(test_control_unit                  single_cycle/test_control_unit.cpp)
add_cpu_test(test_alu_interface                 test_alu_interface.cpp)
add_cpu_test(test_alu_operand_mux               test_alu_operand_mux.cpp)
add_cpu_test(test_single_cycle_datapath         single_cycle/test_single_cycle_datapath.cpp ${PROJECT_SOURCE_DIR}/src/InstructionDecoder.cpp)
add_cpu_test(test_data_memory                   test_data_memory.cpp)
add_cpu_test(test_writeback_mux                 test_writeback_mux.cpp)
add_cpu_test(test_multi_cycle_registers         multi_cycle/test_multi_cycle_registers.cpp)
add_cpu_test(test_multi_cycle_muxes             multi_cycle/test_multi_cycle_muxes.cpp)
add_cpu_test(test_multi_cycle_datapath          multi_cycle/test_multi_cycle_datapath.cpp ${PROJECT_SOURCE_DIR}/src/InstructionDecoder.cpp)
add_cpu_test(test_fsm_control_unit              multi_cycle/test_fsm_control_unit.cpp ${PROJECT_SOURCE_DIR}/src/InstructionDecoder.cpp)
add_cpu_test(test_pipelined_registers           pipelined/test_pipelined_registers.cpp)
add_cpu_test(test_pipelined_muxes               pipelined/test_pipelined_muxes.cpp)
add_cpu_test(test_pipelined_datapath            pipelined/test_pipelined_datapath.cpp ${PROJECT_SOURCE_DIR}/src/InstructionDecoder.cpp)
add_cpu_test(test_cpu_wrappers                  test_cpu_wrappers.cpp ${PROJECT_SOURCE_DIR}/src/InstructionDecoder.cpp)

# ------------------------------------------------------------------------------
# Superscalar Core Tests
# ------------------------------------------------------------------------------
add_cpu_test(test_cache_line                    superscalar/test_cache_line.cpp)
add_cpu_test(test_memory_refill_integration     superscalar/test_memory_refill_integration.cpp)
add_cpu_test(test_fetch_unit                    superscalar/test_fetch_unit.cpp)
add_cpu_test(test_fetch_icache_integration      superscalar/test_fetch_icache_integration.cpp)
add_cpu_test(FetchDecodeRegisterTests           superscalar/FetchDecodeRegisterTests.cpp)
add_cpu_test(DecodeUnitTests                    superscalar/DecodeUnitTests.cpp)
add_cpu_test(RenameUnitTests                    superscalar/RenameUnitTests.cpp)
add_cpu_test(DispatchUnitTests                  superscalar/DispatchUnitTests.cpp)
add_cpu_test(IssueROBTests                      superscalar/IssueROBTests.cpp)
add_cpu_test(IssueQueueTests                    superscalar/IssueQueueTests.cpp)
add_cpu_test(ALUExecutionUnitTests              superscalar/ALUExecutionUnitTests.cpp)
add_cpu_test(IntegerExecutionPipelineTests      superscalar/IntegerExecutionPipelineTests.cpp)
add_cpu_test(BranchExecutionUnitTests           superscalar/BranchExecutionUnitTests.cpp)
add_cpu_test(test_register_alias_table          superscalar/RegisterAliasTableTests.cpp)
add_cpu_test(PhysicalRegisterFreeListRecoveryTests superscalar/PhysicalRegisterFreeListRecoveryTests.cpp)
add_cpu_test(BranchRecoveryUnitTests            superscalar/BranchRecoveryUnitTests.cpp)
add_cpu_test(BranchPredictionUnitTests          superscalar/BranchPredictionUnitTests.cpp)
add_cpu_test(BranchMispredictionIntegrationTests superscalar/BranchMispredictionIntegrationTests.cpp)
add_cpu_test(CommitUnitTests                    superscalar/CommitUnitTests.cpp)
add_cpu_test(LSQMemoryTests                     superscalar/LSQMemoryTests.cpp)
add_cpu_test(SuperScalarCPUTests                superscalar/SuperScalarCPUTests.cpp)
add_cpu_test(DataCacheTests                     superscalar/DataCacheTests.cpp)
add_cpu_test(SuperScalarBenchmarkTests          superscalar/SuperScalarBenchmarkTests.cpp)

# ------------------------------------------------------------------------------
# Vector CPU Tests
# ------------------------------------------------------------------------------
add_cpu_test(test_vector_register               vector/test_vector_register.cpp)
add_cpu_test(test_vector_register_file          vector/test_vector_register_file.cpp)
add_cpu_test(test_minimum_unit                  vector/test_minimum_unit.cpp)
add_cpu_test(test_vlmax_unit                    vector/test_vlmax_unit.cpp)
add_cpu_test(test_vector_configuration_unit     vector/test_vector_configuration_unit.cpp)
add_cpu_test(test_vector_lane_mask_unit         vector/test_vector_lane_mask_unit.cpp)
add_cpu_test(test_vector_masked_wb_unit         vector/test_vector_masked_wb_unit.cpp)
add_cpu_test(test_vector_lane                   vector/test_vector_lane.cpp)
add_cpu_test(test_vector_execution_unit         vector/test_vector_execution_unit.cpp)
