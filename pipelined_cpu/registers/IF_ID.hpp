#pragma once

#include <cstddef>
#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>

namespace cpu {

    template <std::size_t InstructionWidth = 32, std::size_t AddressWidth = 32>
    class IF_ID : public logic::Component {
    public:
        IF_ID(
            logic::Bus<InstructionWidth>& instruction_in,
            logic::Bus<AddressWidth>& pcplus4_in,
            logic::Wire& clock,
            logic::Wire& reset,
            logic::Wire& enable
        ) : instruction_in_(instruction_in),
            pcplus4_in_(pcplus4_in),
            clock_(clock),
            reset_(reset),
            enable_(enable),
            enable_mux_instr(instruction_out_, instruction_in_, enable_, enable_out_instr_),
            reset_mux_instr(enable_out_instr_, zero_instr_, reset_, instr_reg_in_),
            instruction_register_(instr_reg_in_, clock_, instruction_out_),
            enable_mux_pc(pcplus4_out_, pcplus4_in_, enable_, enable_out_pc_),
            reset_mux_pc(enable_out_pc_, zero_pc_, reset_, pcplus4_reg_in_),
            pcplus4_register_(pcplus4_reg_in_, clock_, pcplus4_out_)
        {
            zero_instr_.write(logic::LogicState::LOW);
            zero_pc_.write(logic::LogicState::LOW);
        }

        void evaluate() noexcept override {
            // Instruction part
            enable_mux_instr.evaluate();
            reset_mux_instr.evaluate();
            instruction_register_.evaluate();

            // PC part
            enable_mux_pc.evaluate();
            reset_mux_pc.evaluate();
            pcplus4_register_.evaluate();
        }

        [[nodiscard]]
        logic::Bus<InstructionWidth>& instruction() noexcept {
            return instruction_out_;
        }

        [[nodiscard]]
        const logic::Bus<InstructionWidth>& instruction() const noexcept {
            return instruction_out_;
        }

        [[nodiscard]]
        const logic::Bus<InstructionWidth>& read_instruction() const noexcept {
            return instruction_out_;
        }

        [[nodiscard]]
        logic::Bus<AddressWidth>& pcplus4() noexcept {
            return pcplus4_out_;
        }

        [[nodiscard]]
        const logic::Bus<AddressWidth>& pcplus4() const noexcept {
            return pcplus4_out_;
        }

        [[nodiscard]]
        const logic::Bus<AddressWidth>& read_pcplus4() const noexcept {
            return pcplus4_out_;
        }

    private:
        // Inputs
        logic::Bus<InstructionWidth>& instruction_in_;
        logic::Bus<AddressWidth>& pcplus4_in_;

        // Signals
        logic::Wire& clock_;
        logic::Wire& reset_;
        logic::Wire& enable_;

        // Outputs
        logic::Bus<InstructionWidth> instruction_out_;
        logic::Bus<AddressWidth> pcplus4_out_;

        // Internal buses
        logic::Bus<InstructionWidth> zero_instr_;
        logic::Bus<AddressWidth> zero_pc_;
        logic::Bus<InstructionWidth> enable_out_instr_;
        logic::Bus<AddressWidth> enable_out_pc_;
        logic::Bus<InstructionWidth> instr_reg_in_;
        logic::Bus<AddressWidth> pcplus4_reg_in_;

        // Multiplexers
        logic::Mux<InstructionWidth> enable_mux_instr;
        logic::Mux<InstructionWidth> reset_mux_instr;
        logic::Mux<AddressWidth> enable_mux_pc;
        logic::Mux<AddressWidth> reset_mux_pc;

        // Pipeline registers
        logic::Register<InstructionWidth> instruction_register_;
        logic::Register<AddressWidth> pcplus4_register_;
    };

} // namespace cpu