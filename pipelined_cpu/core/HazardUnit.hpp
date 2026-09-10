/*
    HazardUnit.hpp

    Hazard Detection and Forwarding Unit for the 5-Stage Pipelined Processor
    based on the ARM pipeline architecture 

    it is responsible for:
     Data Forwarding:
       - ForwardAE: Forwards ALU operand A from MEM (10) or WB (01) stage if hazard detected.
       - ForwardBE: Forwards ALU operand B from MEM (10) or WB (01) stage if hazard detected.
     Load-Use Hazard Detection:
       - Stalls IF and ID stages (StallF, StallD) and flushes EX stage (FlushE) when a load
         instruction is followed by a dependent instruction.
    Control Hazard (Branch Flush):
       - Flushes IF/ID (FlushD) and ID/EX (FlushE) registers when a branch is taken.
*/

#pragma once

#include <cstddef>
#include <cstdint>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu {

template <std::size_t RegAddrWidth = 4>
class HazardUnit : public logic::Component {
public:
    HazardUnit(
        // Inputs from Decode stage
        logic::Bus<RegAddrWidth>& ra1_d,
        logic::Bus<RegAddrWidth>& ra2_d,

        // Inputs from Execute stage
        logic::Bus<RegAddrWidth>& ra1_e,
        logic::Bus<RegAddrWidth>& ra2_e,
        logic::Bus<RegAddrWidth>& wa3_e,
        logic::Wire& mem_to_reg_e,
        logic::Wire& branch_taken_e,

        // Inputs from Memory stage
        logic::Bus<RegAddrWidth>& wa3_m,
        logic::Wire& reg_write_m,

        // Inputs from Writeback stage
        logic::Bus<RegAddrWidth>& wa3_w,
        logic::Wire& reg_write_w,

        // Outputs to Execute stage (Forwarding mux controls)
        logic::Bus<2>& forward_ae,
        logic::Bus<2>& forward_be,

        // Outputs to Pipeline control
        logic::Wire& stall_f,
        logic::Wire& stall_d,
        logic::Wire& flush_d,
        logic::Wire& flush_e
    ) : ra1_d_(ra1_d),
        ra2_d_(ra2_d),
        ra1_e_(ra1_e),
        ra2_e_(ra2_e),
        wa3_e_(wa3_e),
        mem_to_reg_e_(mem_to_reg_e),
        branch_taken_e_(branch_taken_e),
        wa3_m_(wa3_m),
        reg_write_m_(reg_write_m),
        wa3_w_(wa3_w),
        reg_write_w_(reg_write_w),
        forward_ae_(forward_ae),
        forward_be_(forward_be),
        stall_f_(stall_f),
        stall_d_(stall_d),
        flush_d_(flush_d),
        flush_e_(flush_e)
    {
    }

    void evaluate() noexcept override {
        const auto ra1_e_val = ra1_e_.read_value();
        const auto ra2_e_val = ra2_e_.read_value();
        const auto wa3_e_val = wa3_e_.read_value();
        const auto wa3_m_val = wa3_m_.read_value();
        const auto wa3_w_val = wa3_w_.read_value();
        const auto ra1_d_val = ra1_d_.read_value();
        const auto ra2_d_val = ra2_d_.read_value();

        const bool reg_write_m_active = (reg_write_m_.read() == logic::LogicState::HIGH);
        const bool reg_write_w_active = (reg_write_w_.read() == logic::LogicState::HIGH);
        const bool mem_to_reg_e_active = (mem_to_reg_e_.read() == logic::LogicState::HIGH);
        const bool branch_taken_active = (branch_taken_e_.read() == logic::LogicState::HIGH);

        // Forward A logic:
        // 10 from MEM stage (ALUOutM)
        // 01 from WB stage (ResultW)
        // 00 from ID/EX register (RD1)
        if (reg_write_m_active && (wa3_m_val == ra1_e_val)) {
            forward_ae_.write_value(0b10);
        } else if (reg_write_w_active && (wa3_w_val == ra1_e_val)) {
            forward_ae_.write_value(0b01);
        } else {
            forward_ae_.write_value(0b00);
        }

        // Forward B logic:
        // 10 from MEM stage (ALUOutM)
        // 01 from WB stage (ResultW)
        // 00 from ID/EX register (RD2)
        if (reg_write_m_active && (wa3_m_val == ra2_e_val)) {
            forward_be_.write_value(0b10);
        } else if (reg_write_w_active && (wa3_w_val == ra2_e_val)) {
            forward_be_.write_value(0b01);
        } else {
            forward_be_.write_value(0b00);
        }

        // Load-Use hazard detection
        const bool ldr_stall = mem_to_reg_e_active &&
                               ((wa3_e_val == ra1_d_val) || (wa3_e_val == ra2_d_val));

        // Control and stall/flush outputs
        if (branch_taken_active) {
            // Branch penalty: flush instructions currently in IF/ID and ID/EX
            stall_f_.write(logic::LogicState::LOW);
            stall_d_.write(logic::LogicState::LOW);
            flush_d_.write(logic::LogicState::HIGH);
            flush_e_.write(logic::LogicState::HIGH);
        } else if (ldr_stall) {
            // Load-use stall: stall IF and ID, insert a bubble (flush) in EX
            stall_f_.write(logic::LogicState::HIGH);
            stall_d_.write(logic::LogicState::HIGH);
            flush_d_.write(logic::LogicState::LOW);
            flush_e_.write(logic::LogicState::HIGH);
        } else {
            stall_f_.write(logic::LogicState::LOW);
            stall_d_.write(logic::LogicState::LOW);
            flush_d_.write(logic::LogicState::LOW);
            flush_e_.write(logic::LogicState::LOW);
        }
    }

private:
    logic::Bus<RegAddrWidth>& ra1_d_;
    logic::Bus<RegAddrWidth>& ra2_d_;

    logic::Bus<RegAddrWidth>& ra1_e_;
    logic::Bus<RegAddrWidth>& ra2_e_;
    logic::Bus<RegAddrWidth>& wa3_e_;
    logic::Wire& mem_to_reg_e_;
    logic::Wire& branch_taken_e_;

    logic::Bus<RegAddrWidth>& wa3_m_;
    logic::Wire& reg_write_m_;

    logic::Bus<RegAddrWidth>& wa3_w_;
    logic::Wire& reg_write_w_;

    logic::Bus<2>& forward_ae_;
    logic::Bus<2>& forward_be_;

    logic::Wire& stall_f_;
    logic::Wire& stall_d_;
    logic::Wire& flush_d_;
    logic::Wire& flush_e_;
};

} 