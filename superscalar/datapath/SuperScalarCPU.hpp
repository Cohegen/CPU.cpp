#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>

// ISA definitions
#include "../../include/isa/Opcode.hpp"
#include "../../include/isa/Registers.hpp"

// Frontend
#include "fetch/FetchUnit.hpp"
#include "fetch/FetchDecodeReg.hpp"
#include "decode/DecodeUnit.hpp"
#include "decode/DecodeBundle.hpp"

// Branch Prediction & Recovery
#include "branch/BranchPredictor.hpp"
#include "branch/BranchPredictionUnit.hpp"
#include "branch/BranchExecutionUnit.hpp"
#include "branch/BranchRecoveryUnit.hpp"
#include "branch/BranchResult.hpp"

// Rename
#include "rename/RegisterAliasTable.hpp"
#include "rename/PhysicalRegisterFreeList.hpp"
#include "rename/RenameUnit.hpp"
#include "rename/RenameBundle.hpp"

// Dispatch, ROB & Issue
#include "dispatch/ReOrderBuffer.hpp"
#include "dispatch/ROBEntry.hpp"
#include "dispatch/DispatchUnit.hpp"
#include "issue/IssueQueue.hpp"
#include "issue/IssueQueueEntry.hpp"
#include "issue/PhysicalRegisterFile.hpp"

// Execution Pipelines
#include "execution/ALUExecutionUnit.hpp"
#include "execution/MemoryExecutionUnit.hpp"

// Memory & Disambiguation
#include "memory/LoadStoreQueue.hpp"
#include "memory/data/DataMemory.hpp"
#include "memory/instruction/SuperScalarInstructionMemory.hpp"

// Commit & Retirement
#include "commit/StoreBuffer.hpp"
#include "commit/ArchitecturalRegisterFile.hpp"
#include "commit/CommitUnit.hpp"

namespace cpu
{

template <
    std::size_t ROBCapacity = 16,
    std::size_t IQCapacity = 16,
    std::size_t PRFCount = 64,
    std::size_t ARFCount = 16,
    std::size_t LSQCapacity = 32,
    std::size_t StoreBufferCapacity = 16,
    std::size_t DataMemoryWords = 1024,
    std::size_t BPTableSize = 256,
    std::size_t ROMAddressWidth = 8
>
class SuperScalarCPU
{
public:
    using ROB = ReOrderBuffer<ROBCapacity>;
    using IQ  = IssueQueue<IQCapacity>;
    using PRF = PhysicalRegisterFile<PRFCount>;
    using ARF = ArchitecturalRegisterFile<ARFCount>;
    using LSQ = LoadStoreQueue<LSQCapacity>;
    using SB  = StoreBuffer<StoreBufferCapacity>;
    using DM  = DataMemory<DataMemoryWords>;
    using BPU = BranchPredictionUnit<BPTableSize>;
    using Predictor = BranchPredictor<BPTableSize>;
    using IM  = SuperScalarInstructionMemory<32, ROMAddressWidth, 32>;

    struct CycleStats
    {
        std::size_t cycle{0};
        std::size_t fetched{0};
        std::size_t decoded{0};
        std::size_t renamed{0};
        std::size_t dispatched{0};
        std::size_t issued_alu{0};
        std::size_t issued_branch{0};
        std::size_t issued_mem{0};
        std::size_t retired{0};
        bool branch_mispredicted{false};
        std::uint32_t redirect_pc{0};
        bool halted{false};
    };

    SuperScalarCPU()
        : free_list_(PRFCount, ARFCount),
          rat_(PRFCount),
          rename_unit_(rat_, free_list_, prf_),
          dispatch_unit_(rob_, iq_, prf_),
          commit_unit_(2),
          imem_(imem_req0_, imem_req1_, imem_addr0_, imem_addr1_,
                imem_inst0_, imem_inst1_, imem_resp0_, imem_resp1_),
          fetch_unit_(clock_, reset_, fetch_pc0_, fetch_pc1_,
                      fetch_inst0_, fetch_inst1_, fetch_hit0_, fetch_hit1_,
                      fetch_valid0_, fetch_valid1_, fetch_next_pc_,
                      fetch_redirect_pc_, fetch_redirect_),
          fd_reg_(clock_, reset_, fd_enable_, fd_flush_,
                  fetch_pc0_, fetch_inst0_, fetch_valid0_,
                  fetch_pc1_, fetch_inst1_, fetch_valid1_,
                  decode_pc0_, decode_inst0_, decode_valid0_,
                  decode_pc1_, decode_inst1_, decode_valid1_),
          mem_unit_(rob_, iq_, prf_, lsq_, store_buffer_, data_memory_),
          bpu_(predictor_)
    {
        reset();
    }

    void reset() noexcept
    {
        cycles_ = 0;
        halted_ = false;
        halt_dispatched_ = false;

        // Reset stateful hardware components
        rob_.reset();
        iq_.reset();
        prf_.reset();
        arf_.reset();
        rat_.reset();
        free_list_.reset();
        store_buffer_.reset();
        lsq_.reset();
        data_memory_.reset();
        predictor_.reset();
        bru_.reset();
        commit_unit_.reset();

        // Architectural registers R0..R15 are mapped to P0..P15 in RAT initially.
        // PRF entries P0..P15 hold the committed architectural values (0) and are ready.
        for (std::size_t i = 0; i < ARFCount; ++i)
        {
            prf_.set_ready(static_cast<std::uint8_t>(i), true);
            prf_.write(static_cast<std::uint8_t>(i), 0);
        }

        // Signal initializations
        clock_.write(logic::LogicState::LOW);
        reset_.write(logic::LogicState::HIGH);
        fd_enable_.write(logic::LogicState::HIGH);
        fd_flush_.write(logic::LogicState::LOW);
        fetch_hit0_.write(logic::LogicState::HIGH);
        fetch_hit1_.write(logic::LogicState::HIGH);
        fetch_redirect_.write(logic::LogicState::LOW);

        // Run evaluation cycle under reset to clear internal registers
        eval_clock_edge();

        reset_.write(logic::LogicState::LOW);
        clock_.write(logic::LogicState::LOW);
        fetch_unit_.evaluate();
        fd_reg_.evaluate();
    }

    void load_program(const std::vector<std::size_t>& words) noexcept
    {
        imem_.load(words);
        reset();
    }

    void set_data_word(std::size_t word_index, std::uint32_t value) noexcept
    {
        data_memory_.write_word(word_index, value);
    }

    [[nodiscard]]
    std::uint32_t get_data_word(std::size_t word_index) const noexcept
    {
        return data_memory_.read_word(word_index);
    }

    [[nodiscard]]
    std::uint32_t read_arf(Register reg) const noexcept
    {
        return arf_.read(reg);
    }

    [[nodiscard]]
    bool halted() const noexcept
    {
        return halted_;
    }

    [[nodiscard]]
    std::size_t cycle_count() const noexcept
    {
        return cycles_;
    }

    [[nodiscard]]
    std::size_t committed_instruction_count() const noexcept
    {
        return commit_unit_.committed_instruction_count();
    }

    // Advance the CPU by one full clock cycle
    CycleStats step() noexcept
    {
        CycleStats stats{};
        stats.cycle = cycles_++;

        if (halted_)
        {
            stats.halted = true;
            return stats;
        }

        // ====================================================================
        // STAGE 7: COMMIT & RETIREMENT (In-order, at ROB head)
        // ====================================================================
        auto commit_events = commit_unit_.step(rob_, prf_, arf_, free_list_, rat_, &store_buffer_);
        stats.retired = commit_events.size();

        for (const auto& ev : commit_events)
        {
            if (ev.store)
            {
                // Pop the committed store from StoreBuffer and write to DataMemory
                auto committed_store = store_buffer_.pop_committed();
                if (committed_store.has_value())
                {
                    data_memory_.write(committed_store->address, committed_store->data);
                }
            }

            // Remove retired entry from LSQ
            lsq_.remove(ev.rob_index);

            if (ev.halt)
            {
                halted_ = true;
                stats.halted = true;
            }
        }

        // ====================================================================
        // STAGE 6: EXECUTION & WRITEBACK
        // ====================================================================
        // (A) Memory Execution Unit (LW/SW)
        auto mem_results = mem_unit_.execute();
        stats.issued_mem = mem_results.size();

        // (B) Branch Execution Unit
        bool branch_recovered = false;
        for (std::size_t i = 0; i < iq_.capacity(); ++i)
        {
            auto* entry = iq_.entry(i);
            if (entry != nullptr && entry->valid && !entry->issued &&
                entry->control_flow == ControlFlow::BRANCH &&
                entry->rs1_ready && entry->rs2_ready)
            {
                // Ready branch selected
                stats.issued_branch = 1;
                const auto captured_entry = *entry;
                iq_.remove(i);

                BranchResult b_res = branch_unit_.evaluate(
                    captured_entry, captured_entry.predicted_taken, captured_entry.predicted_target
                );

                // Update predictor with actual branch outcome
                predictor_.update(b_res.pc, b_res.taken, b_res.target_pc);

                // Mark completed in ROB
                rob_.complete(captured_entry.rob_index, b_res.target_pc);

                // Check for misprediction
                if (b_res.mispredicted)
                {
                    stats.branch_mispredicted = true;
                    stats.redirect_pc = b_res.redirect_pc;
                    halt_dispatched_ = false;

                    // Branch Recovery: restore RAT, squash younger ROB & IQ entries
                    (void)bru_.recover(b_res, rob_, rat_, free_list_, iq_);

                    // Also squash younger LSQ and StoreBuffer entries
                    std::vector<typename ROB::Index> squashed_indices;
                    (void)rob_.squash_younger_than(b_res.rob_index, squashed_indices);
                    lsq_.squash(squashed_indices);
                    store_buffer_.squash(squashed_indices);

                    // Redirect fetch & flush frontend
                    fetch_redirect_.write(logic::LogicState::HIGH);
                    fetch_redirect_pc_.write_value(b_res.redirect_pc);
                    fd_flush_.write(logic::LogicState::HIGH);

                    branch_recovered = true;
                }

                break; // 1 branch executed per cycle
            }
        }

        // (C) ALU Execution (Up to 2 integer operations per cycle)
        for (std::size_t lane = 0; lane < 2; ++lane)
        {
            auto* entry = select_ready_alu_entry();
            if (entry == nullptr)
            {
                break;
            }

            ++stats.issued_alu;
            const auto captured_entry = *entry;
            iq_.remove(entry);

            // Compute operands
            const std::uint32_t op_a = (captured_entry.operand_a == OperandSource::REGISTER)
                ? captured_entry.rs1_value
                : 0U;
            const std::uint32_t op_b = (captured_entry.operand_b == OperandSource::REGISTER)
                ? captured_entry.rs2_value
                : static_cast<std::uint32_t>(captured_entry.immediate);

            std::uint32_t alu_res = 0;
            switch (captured_entry.alu_operation)
            {
                case ALUOperation::ADD:
                    alu_res = op_a + op_b;
                    break;
                case ALUOperation::SUB:
                    alu_res = op_a - op_b;
                    break;
                case ALUOperation::AND:
                    alu_res = op_a & op_b;
                    break;
                case ALUOperation::OR:
                    alu_res = op_a | op_b;
                    break;
                case ALUOperation::XOR:
                    alu_res = op_a ^ op_b;
                    break;
                case ALUOperation::NOT:
                    alu_res = ~op_a;
                    break;
                default:
                    alu_res = 0;
                    break;
            }

            // Write back to PRF, IssueQueue wakeup, and mark ROB completed
            if (captured_entry.register_write)
            {
                prf_.write(captured_entry.physical_rd, alu_res);
                iq_.wakeup(captured_entry.physical_rd, alu_res);
            }
            rob_.complete(captured_entry.rob_index, alu_res);
        }

        // ====================================================================
        // STAGE 4 & 5: RENAME & DISPATCH
        // ====================================================================
        bool frontend_redirect_active = false;
        if (!branch_recovered && !halt_dispatched_)
        {
            const auto dec_bundle0 = decode_unit_.decode(
                static_cast<std::uint32_t>(decode_pc0_.read_value()),
                static_cast<std::uint32_t>(decode_inst0_.read_value()),
                decode_valid0_.read() == logic::LogicState::HIGH
            );
            const auto dec_bundle1 = decode_unit_.decode(
                static_cast<std::uint32_t>(decode_pc1_.read_value()),
                static_cast<std::uint32_t>(decode_inst1_.read_value()),
                decode_valid1_.read() == logic::LogicState::HIGH
            );

            if (dec_bundle0.valid) ++stats.decoded;
            if (dec_bundle1.valid) ++stats.decoded;

            // Branch prediction annotations from BPU
            auto pred_res = bpu_.evaluate(
                dec_bundle0.pc, dec_bundle0.valid, dec_bundle0.control_flow,
                dec_bundle1.pc, dec_bundle1.valid, dec_bundle1.control_flow
            );

            auto ren_input0 = dec_bundle0;
            if (ren_input0.control_flow == ControlFlow::BRANCH)
            {
                ren_input0.prediction.valid = true;
                ren_input0.prediction.predicted_taken = pred_res.lane0.taken;
                ren_input0.prediction.predicted_target = pred_res.lane0.target;
            }

            auto ren_input1 = dec_bundle1;
            if (ren_input1.control_flow == ControlFlow::BRANCH)
            {
                ren_input1.prediction.valid = true;
                ren_input1.prediction.predicted_taken = pred_res.lane1.taken;
                ren_input1.prediction.predicted_target = pred_res.lane1.target;
            }

            // Frontend branch redirection on predicted taken branch
            if (pred_res.redirect)
            {
                fetch_redirect_.write(logic::LogicState::HIGH);
                fetch_redirect_pc_.write_value(pred_res.redirect_pc);
                fd_flush_.write(logic::LogicState::HIGH);
                frontend_redirect_active = true;

                if (pred_res.redirect_lane == 0)
                {
                    ren_input1.valid = false;
                }
            }

            // Check if Rename and Dispatch have capacity
            if (rename_unit_.can_rename(ren_input0, ren_input1))
            {
                RenameBundle ren_out0{}, ren_out1{};
                rename_unit_.rename(ren_input0, ren_input1, ren_out0, ren_out1);

                if (ren_out0.valid)
                {
                    std::size_t r_idx0 = 0;
                    auto rat_cp0 = rat_.checkpoint();
                    const auto* cp_ptr0 = (ren_out0.control_flow == ControlFlow::BRANCH) ? &rat_cp0 : nullptr;

                    if (dispatch_unit_.dispatch(ren_out0, r_idx0, cp_ptr0))
                    {
                        ++stats.dispatched;
                        if (ren_out0.memory_read)
                        {
                            (void)lsq_.allocate(LSQ::OpType::LOAD, r_idx0, ren_out0.pc, ren_out0.physical_rd);
                        }
                        else if (ren_out0.memory_write)
                        {
                            (void)lsq_.allocate(LSQ::OpType::STORE, r_idx0, ren_out0.pc);
                            (void)store_buffer_.allocate(r_idx0, ren_out0.pc);
                        }
                        else if (ren_out0.halt)
                        {
                            rob_.complete(r_idx0, 0);
                            halt_dispatched_ = true;
                            remove_from_iq(r_idx0);
                        }
                        else if (ren_out0.opcode == Opcode::NOP)
                        {
                            rob_.complete(r_idx0, 0);
                            remove_from_iq(r_idx0);
                        }
                    }
                }

                if (!halt_dispatched_ && ren_out1.valid)
                {
                    std::size_t r_idx1 = 0;
                    auto rat_cp1 = rat_.checkpoint();
                    const auto* cp_ptr1 = (ren_out1.control_flow == ControlFlow::BRANCH) ? &rat_cp1 : nullptr;

                    if (dispatch_unit_.dispatch(ren_out1, r_idx1, cp_ptr1))
                    {
                        ++stats.dispatched;
                        if (ren_out1.memory_read)
                        {
                            (void)lsq_.allocate(LSQ::OpType::LOAD, r_idx1, ren_out1.pc, ren_out1.physical_rd);
                        }
                        else if (ren_out1.memory_write)
                        {
                            (void)lsq_.allocate(LSQ::OpType::STORE, r_idx1, ren_out1.pc);
                            (void)store_buffer_.allocate(r_idx1, ren_out1.pc);
                        }
                        else if (ren_out1.halt)
                        {
                            rob_.complete(r_idx1, 0);
                            halt_dispatched_ = true;
                            remove_from_iq(r_idx1);
                        }
                        else if (ren_out1.opcode == Opcode::NOP)
                        {
                            rob_.complete(r_idx1, 0);
                            remove_from_iq(r_idx1);
                        }
                    }
                }
            }
        }

        // ====================================================================
        // STAGE 1, 2 & 3: FETCH, MEMORY & DECODE REG
        // ====================================================================
        // Evaluate IMEM requests from FetchUnit
        imem_req0_.write(logic::LogicState::HIGH);
        imem_req1_.write(logic::LogicState::HIGH);
        copy_bus_helper(fetch_pc0_, imem_addr0_);
        copy_bus_helper(fetch_pc1_, imem_addr1_);
        imem_.evaluate();

        copy_bus_helper(imem_inst0_, fetch_inst0_);
        copy_bus_helper(imem_inst1_, fetch_inst1_);
        fetch_hit0_.write(imem_resp0_.read());
        fetch_hit1_.write(imem_resp1_.read());
        fetch_unit_.evaluate();

        // Clock the hardware pipeline registers (Fetch & FetchDecodeReg)
        eval_clock_edge();

        // Clear one-shot pulse signals after clock edge
        if (branch_recovered || frontend_redirect_active)
        {
            fetch_redirect_.write(logic::LogicState::LOW);
            fd_flush_.write(logic::LogicState::LOW);
        }

        return stats;
    }

    // Run until HALT or max_cycles
    std::size_t run(std::size_t max_cycles = 1000) noexcept
    {
        while (!halted_ && cycles_ < max_cycles)
        {
            step();
        }
        return cycles_;
    }

    void remove_from_iq(std::size_t rob_index) noexcept
    {
        for (std::size_t i = 0; i < iq_.capacity(); ++i)
        {
            auto* e = iq_.entry(i);
            if (e != nullptr && e->valid && e->rob_index == rob_index)
            {
                iq_.remove(i);
                break;
            }
        }
    }

    [[nodiscard]]
    IssueQueueEntry* select_ready_alu_entry() noexcept
    {
        for (std::size_t i = 0; i < iq_.capacity(); ++i)
        {
            auto* entry = iq_.entry(i);
            if (entry != nullptr && entry->valid && !entry->issued &&
                entry->rs1_ready && entry->rs2_ready &&
                !entry->memory_read && !entry->memory_write &&
                entry->control_flow == ControlFlow::NONE && !entry->halt &&
                entry->alu_operation != ALUOperation::NONE)
            {
                return entry;
            }
        }
        return nullptr;
    }

    void eval_clock_edge() noexcept
    {
        // Falling edge: latches input
        clock_.write(logic::LogicState::LOW);
        fetch_unit_.evaluate();
        fd_reg_.evaluate();

        // Rising edge: transfers to outputs
        clock_.write(logic::LogicState::HIGH);
        fetch_unit_.evaluate();
        fd_reg_.evaluate();
    }

    template <std::size_t Width>
    static void copy_bus_helper(const logic::Bus<Width>& src, logic::Bus<Width>& dst) noexcept
    {
        for (std::size_t i = 0; i < Width; ++i)
        {
            dst[i].write(src[i].read());
        }
    }

    // Core Data Structures
    ROB rob_{};
    IQ  iq_{};
    PRF prf_{};
    ARF arf_{};
    LSQ lsq_{};
    SB  store_buffer_{};
    DM  data_memory_{};
    PhysicalRegisterFreeList free_list_;
    RegisterAliasTable rat_;
    Predictor predictor_{};

    // Sub-units
    RenameUnit<PRFCount> rename_unit_;
    DispatchUnit<ROBCapacity, IQCapacity, PRFCount> dispatch_unit_;
    CommitUnit<ROBCapacity, PRFCount, ARFCount, StoreBufferCapacity> commit_unit_;
    BranchExecutionUnit<32> branch_unit_{};
    BranchRecoveryUnit<ROBCapacity, IQCapacity> bru_{};
    DecodeUnit decode_unit_{};
    MemoryExecutionUnit<ROBCapacity, IQCapacity, PRFCount, LSQCapacity, StoreBufferCapacity, DataMemoryWords> mem_unit_;
    BPU bpu_;

    // Instruction Memory & Bus Signals
    logic::Wire imem_req0_{logic::LogicState::LOW};
    logic::Wire imem_req1_{logic::LogicState::LOW};
    logic::Bus<32> imem_addr0_{};
    logic::Bus<32> imem_addr1_{};
    logic::Bus<32> imem_inst0_{};
    logic::Bus<32> imem_inst1_{};
    logic::Wire imem_resp0_{logic::LogicState::LOW};
    logic::Wire imem_resp1_{logic::LogicState::LOW};
    IM imem_;

    // Clock and Control Signals
    logic::Wire clock_{logic::LogicState::LOW};
    logic::Wire reset_{logic::LogicState::LOW};
    logic::Wire fd_enable_{logic::LogicState::HIGH};
    logic::Wire fd_flush_{logic::LogicState::LOW};

    // Fetch Signals
    logic::Bus<32> fetch_pc0_{};
    logic::Bus<32> fetch_pc1_{};
    logic::Bus<32> fetch_inst0_{};
    logic::Bus<32> fetch_inst1_{};
    logic::Wire fetch_hit0_{logic::LogicState::HIGH};
    logic::Wire fetch_hit1_{logic::LogicState::HIGH};
    logic::Wire fetch_valid0_{logic::LogicState::LOW};
    logic::Wire fetch_valid1_{logic::LogicState::LOW};
    logic::Bus<32> fetch_next_pc_{};
    logic::Bus<32> fetch_redirect_pc_{};
    logic::Wire fetch_redirect_{logic::LogicState::LOW};
    FetchUnit<32, 32> fetch_unit_;

    // Decode Signals
    logic::Bus<32> decode_pc0_{};
    logic::Bus<32> decode_pc1_{};
    logic::Bus<32> decode_inst0_{};
    logic::Bus<32> decode_inst1_{};
    logic::Wire decode_valid0_{logic::LogicState::LOW};
    logic::Wire decode_valid1_{logic::LogicState::LOW};
    FetchDecodeReg<32, 32> fd_reg_;

    // State
    std::size_t cycles_{0};
    bool halted_{false};
    bool halt_dispatched_{false};
};

} // namespace cpu
