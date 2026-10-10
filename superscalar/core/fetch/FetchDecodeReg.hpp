#pragma once

#include <cstddef>
#include <cstdint>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/sequential/registers/register.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>

#include "FetchBundle.hpp"

namespace cpu {

template <
    std::size_t CPUAddressWidth = 32,
    std::size_t InstructionWidth = 32
>
class FetchDecodeReg : public logic::Component {
public:
    FetchDecodeReg(
        logic::Wire& clock,
        logic::Wire& reset,
        logic::Wire& enable,
        logic::Wire& flush,

        logic::Bus<CPUAddressWidth>& fetch_pc0,
        logic::Bus<InstructionWidth>& fetch_instruction0,
        logic::Wire& fetch_valid0,

        logic::Bus<CPUAddressWidth>& fetch_pc1,
        logic::Bus<InstructionWidth>& fetch_instruction1,
        logic::Wire& fetch_valid1,

        logic::Bus<CPUAddressWidth>& decode_pc0,
        logic::Bus<InstructionWidth>& decode_instruction0,
        logic::Wire& decode_valid0,

        logic::Bus<CPUAddressWidth>& decode_pc1,
        logic::Bus<InstructionWidth>& decode_instruction1,
        logic::Wire& decode_valid1
    ) :
        clock_(clock),
        reset_(reset),
        enable_(enable),
        flush_(flush),
        fetch_pc0_(fetch_pc0),
        fetch_instruction0_(fetch_instruction0),
        fetch_valid0_(fetch_valid0),
        fetch_pc1_(fetch_pc1),
        fetch_instruction1_(fetch_instruction1),
        fetch_valid1_(fetch_valid1),
        decode_pc0_(decode_pc0),
        decode_instruction0_(decode_instruction0),
        decode_valid0_(decode_valid0),
        decode_pc1_(decode_pc1),
        decode_instruction1_(decode_instruction1),
        decode_valid1_(decode_valid1),

        // Lane 0: Mux feedback for stall (enable), flush, and clear (reset)
        enable_mux_pc0_(pc0_reg_out_, fetch_pc0_, enable_, enable_out_pc0_),
        flush_mux_pc0_(enable_out_pc0_, zero_pc0_, flush_, flush_out_pc0_),
        reset_mux_pc0_(flush_out_pc0_, zero_pc0_, reset_, pc0_reg_in_),
        pc0_reg_(pc0_reg_in_, clock_, pc0_reg_out_),

        enable_mux_instr0_(instruction0_reg_out_, fetch_instruction0_, enable_, enable_out_instr0_),
        flush_mux_instr0_(enable_out_instr0_, zero_instr0_, flush_, flush_out_instr0_),
        reset_mux_instr0_(flush_out_instr0_, zero_instr0_, reset_, instr0_reg_in_),
        instruction0_reg_(instr0_reg_in_, clock_, instruction0_reg_out_),

        enable_mux_valid0_(valid0_reg_output_, valid0_reg_input_, enable_, enable_out_valid0_),
        flush_mux_valid0_(enable_out_valid0_, zero_valid0_, flush_, flush_out_valid0_),
        reset_mux_valid0_(flush_out_valid0_, zero_valid0_, reset_, valid0_reg_in_),
        valid0_reg_(valid0_reg_in_, clock_, valid0_reg_output_),

        // Lane 1: Mux feedback for stall (enable), flush, and clear (reset)
        enable_mux_pc1_(pc1_reg_out_, fetch_pc1_, enable_, enable_out_pc1_),
        flush_mux_pc1_(enable_out_pc1_, zero_pc1_, flush_, flush_out_pc1_),
        reset_mux_pc1_(flush_out_pc1_, zero_pc1_, reset_, pc1_reg_in_),
        pc1_reg_(pc1_reg_in_, clock_, pc1_reg_out_),

        enable_mux_instr1_(instruction1_reg_out_, fetch_instruction1_, enable_, enable_out_instr1_),
        flush_mux_instr1_(enable_out_instr1_, zero_instr1_, flush_, flush_out_instr1_),
        reset_mux_instr1_(flush_out_instr1_, zero_instr1_, reset_, instr1_reg_in_),
        instruction1_reg_(instr1_reg_in_, clock_, instruction1_reg_out_),

        enable_mux_valid1_(valid1_reg_output_, valid1_reg_input_, enable_, enable_out_valid1_),
        flush_mux_valid1_(enable_out_valid1_, zero_valid1_, flush_, flush_out_valid1_),
        reset_mux_valid1_(flush_out_valid1_, zero_valid1_, reset_, valid1_reg_in_),
        valid1_reg_(valid1_reg_in_, clock_, valid1_reg_output_)
    {
        zero_pc0_.clear();
        zero_instr0_.clear();
        zero_valid0_.clear();

        zero_pc1_.clear();
        zero_instr1_.clear();
        zero_valid1_.clear();

        pc0_reg_out_.clear();
        instruction0_reg_out_.clear();
        valid0_reg_output_.clear();

        pc1_reg_out_.clear();
        instruction1_reg_out_.clear();
        valid1_reg_output_.clear();

        valid0_reg_input_.clear();
        valid1_reg_input_.clear();

        decode_pc0_.clear();
        decode_instruction0_.clear();
        decode_valid0_.write(logic::LogicState::LOW);

        decode_pc1_.clear();
        decode_instruction1_.clear();
        decode_valid1_.write(logic::LogicState::LOW);
    }

    void evaluate() noexcept override
    {
        // 1. Converting valid wire inputs to internal 1-bit buses
        valid0_reg_input_[0].write(fetch_valid0_.read());
        valid1_reg_input_[0].write(fetch_valid1_.read());

        // 2. Evaluating Lane 0 multiplexers
        enable_mux_pc0_.evaluate();
        flush_mux_pc0_.evaluate();
        reset_mux_pc0_.evaluate();
        enable_mux_instr0_.evaluate();
        flush_mux_instr0_.evaluate();
        reset_mux_instr0_.evaluate();
        enable_mux_valid0_.evaluate();
        flush_mux_valid0_.evaluate();
        reset_mux_valid0_.evaluate();

        // 3. Evaluating Lane 1 multiplexers
        enable_mux_pc1_.evaluate();
        flush_mux_pc1_.evaluate();
        reset_mux_pc1_.evaluate();
        enable_mux_instr1_.evaluate();
        flush_mux_instr1_.evaluate();
        reset_mux_instr1_.evaluate();
        enable_mux_valid1_.evaluate();
        flush_mux_valid1_.evaluate();
        reset_mux_valid1_.evaluate();

        // 4. Evaluate sequential registers
        pc0_reg_.evaluate();
        instruction0_reg_.evaluate();
        valid0_reg_.evaluate();

        pc1_reg_.evaluate();
        instruction1_reg_.evaluate();
        valid1_reg_.evaluate();

        // 5. Driving decode outputs
        copy_bus(pc0_reg_out_, decode_pc0_);
        copy_bus(instruction0_reg_out_, decode_instruction0_);
        decode_valid0_.write(valid0_reg_output_[0].read());

        copy_bus(pc1_reg_out_, decode_pc1_);
        copy_bus(instruction1_reg_out_, decode_instruction1_);
        decode_valid1_.write(valid1_reg_output_[0].read());
    }

    [[nodiscard]]
    logic::Bus<CPUAddressWidth>& decode_pc0() noexcept { return decode_pc0_; }
    [[nodiscard]]
    const logic::Bus<CPUAddressWidth>& decode_pc0() const noexcept { return decode_pc0_; }

    [[nodiscard]]
    logic::Bus<InstructionWidth>& decode_instruction0() noexcept { return decode_instruction0_; }
    [[nodiscard]]
    const logic::Bus<InstructionWidth>& decode_instruction0() const noexcept { return decode_instruction0_; }

    [[nodiscard]]
    logic::Wire& decode_valid0() noexcept { return decode_valid0_; }
    [[nodiscard]]
    const logic::Wire& decode_valid0() const noexcept { return decode_valid0_; }

    [[nodiscard]]
    logic::Bus<CPUAddressWidth>& decode_pc1() noexcept { return decode_pc1_; }
    [[nodiscard]]
    const logic::Bus<CPUAddressWidth>& decode_pc1() const noexcept { return decode_pc1_; }

    [[nodiscard]]
    logic::Bus<InstructionWidth>& decode_instruction1() noexcept { return decode_instruction1_; }
    [[nodiscard]]
    const logic::Bus<InstructionWidth>& decode_instruction1() const noexcept { return decode_instruction1_; }

    [[nodiscard]]
    logic::Wire& decode_valid1() noexcept { return decode_valid1_; }
    [[nodiscard]]
    const logic::Wire& decode_valid1() const noexcept { return decode_valid1_; }

    [[nodiscard]]
    FetchBundle get_bundle() const noexcept
    {
        return FetchBundle(
            static_cast<uint32_t>(decode_pc0_.read_value()),
            static_cast<uint32_t>(decode_instruction0_.read_value()),
            decode_valid0_.read() == logic::LogicState::HIGH,
            static_cast<uint32_t>(decode_pc1_.read_value()),
            static_cast<uint32_t>(decode_instruction1_.read_value()),
            decode_valid1_.read() == logic::LogicState::HIGH
        );
    }

private:
    // External control signals
    logic::Wire& clock_;
    logic::Wire& reset_;
    logic::Wire& enable_;
    logic::Wire& flush_;

    // Lane 0 inputs
    logic::Bus<CPUAddressWidth>& fetch_pc0_;
    logic::Bus<InstructionWidth>& fetch_instruction0_;
    logic::Wire& fetch_valid0_;

    // Lane 1 inputs
    logic::Bus<CPUAddressWidth>& fetch_pc1_;
    logic::Bus<InstructionWidth>& fetch_instruction1_;
    logic::Wire& fetch_valid1_;

    // Lane 0 outputs
    logic::Bus<CPUAddressWidth>& decode_pc0_;
    logic::Bus<InstructionWidth>& decode_instruction0_;
    logic::Wire& decode_valid0_;

    // Lane 1 outputs
    logic::Bus<CPUAddressWidth>& decode_pc1_;
    logic::Bus<InstructionWidth>& decode_instruction1_;
    logic::Wire& decode_valid1_;

    // Internal zero constant buses
    logic::Bus<CPUAddressWidth> zero_pc0_;
    logic::Bus<InstructionWidth> zero_instr0_;
    logic::Bus<1> zero_valid0_;

    logic::Bus<CPUAddressWidth> zero_pc1_;
    logic::Bus<InstructionWidth> zero_instr1_;
    logic::Bus<1> zero_valid1_;

    // Internal Mux intermediate buses
    logic::Bus<CPUAddressWidth> enable_out_pc0_;
    logic::Bus<CPUAddressWidth> flush_out_pc0_;
    logic::Bus<CPUAddressWidth> pc0_reg_in_;
    logic::Bus<InstructionWidth> enable_out_instr0_;
    logic::Bus<InstructionWidth> flush_out_instr0_;
    logic::Bus<InstructionWidth> instr0_reg_in_;
    logic::Bus<1> enable_out_valid0_;
    logic::Bus<1> flush_out_valid0_;
    logic::Bus<1> valid0_reg_in_;

    logic::Bus<CPUAddressWidth> enable_out_pc1_;
    logic::Bus<CPUAddressWidth> flush_out_pc1_;
    logic::Bus<CPUAddressWidth> pc1_reg_in_;
    logic::Bus<InstructionWidth> enable_out_instr1_;
    logic::Bus<InstructionWidth> flush_out_instr1_;
    logic::Bus<InstructionWidth> instr1_reg_in_;
    logic::Bus<1> enable_out_valid1_;
    logic::Bus<1> flush_out_valid1_;
    logic::Bus<1> valid1_reg_in_;

    // Lane 0 register outputs & multiplexers
    logic::Bus<CPUAddressWidth> pc0_reg_out_;
    logic::Mux<CPUAddressWidth> enable_mux_pc0_;
    logic::Mux<CPUAddressWidth> flush_mux_pc0_;
    logic::Mux<CPUAddressWidth> reset_mux_pc0_;
    logic::Register<CPUAddressWidth> pc0_reg_;

    logic::Bus<InstructionWidth> instruction0_reg_out_;
    logic::Mux<InstructionWidth> enable_mux_instr0_;
    logic::Mux<InstructionWidth> flush_mux_instr0_;
    logic::Mux<InstructionWidth> reset_mux_instr0_;
    logic::Register<InstructionWidth> instruction0_reg_;

    logic::Bus<1> valid0_reg_input_;
    logic::Bus<1> valid0_reg_output_;
    logic::Mux<1> enable_mux_valid0_;
    logic::Mux<1> flush_mux_valid0_;
    logic::Mux<1> reset_mux_valid0_;
    logic::Register<1> valid0_reg_;

    // Lane 1 register outputs & multiplexers
    logic::Bus<CPUAddressWidth> pc1_reg_out_;
    logic::Mux<CPUAddressWidth> enable_mux_pc1_;
    logic::Mux<CPUAddressWidth> flush_mux_pc1_;
    logic::Mux<CPUAddressWidth> reset_mux_pc1_;
    logic::Register<CPUAddressWidth> pc1_reg_;

    logic::Bus<InstructionWidth> instruction1_reg_out_;
    logic::Mux<InstructionWidth> enable_mux_instr1_;
    logic::Mux<InstructionWidth> flush_mux_instr1_;
    logic::Mux<InstructionWidth> reset_mux_instr1_;
    logic::Register<InstructionWidth> instruction1_reg_;

    logic::Bus<1> valid1_reg_input_;
    logic::Bus<1> valid1_reg_output_;
    logic::Mux<1> enable_mux_valid1_;
    logic::Mux<1> flush_mux_valid1_;
    logic::Mux<1> reset_mux_valid1_;
    logic::Register<1> valid1_reg_;

    template <std::size_t Width>
    static void copy_bus(const logic::Bus<Width>& source, logic::Bus<Width>& destination) noexcept
    {
        for (std::size_t i = 0; i < Width; ++i)
        {
            destination[i].write(source[i].read());
        }
    }
};

} // namespace cpu
