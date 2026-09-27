#pragma once

#include <cstddef>
#include <cstdint>

#include <logic/combinational/adders/RippleCarryAdder.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

#include "FetchBundle.hpp"
#include "../../components/ProgramCounter.hpp"

namespace cpu
{

template <
    std::size_t CPUAddressWidth = 32,
    std::size_t InstructionWidth = 32
>
class FetchUnit : public logic::Component
{
    static_assert(
        CPUAddressWidth > 0,
        "CPUAddressWidth must be greater than zero."
    );

    static_assert(
        InstructionWidth > 0,
        "InstructionWidth must be greater than zero."
    );

    static_assert(
        InstructionWidth % 8 == 0,
        "InstructionWidth must be a multiple of 8."
    );

public:

    static constexpr std::size_t InstructionBytes =
        InstructionWidth / 8;

    static constexpr std::size_t FetchBytes =
        2 * InstructionBytes;

    FetchUnit(
        logic::Wire& clock,
        logic::Wire& reset,

        logic::Bus<CPUAddressWidth>& pc0,
        logic::Bus<CPUAddressWidth>& pc1,

        logic::Bus<InstructionWidth>& instruction0,
        logic::Bus<InstructionWidth>& instruction1,

        logic::Wire& hit0,
        logic::Wire& hit1,

        logic::Wire& valid0,
        logic::Wire& valid1,

        logic::Bus<CPUAddressWidth>& next_pc
    )
        : FetchUnit(
            clock,
            reset,
            pc0,
            pc1,
            instruction0,
            instruction1,
            hit0,
            hit1,
            valid0,
            valid1,
            next_pc,
            nullptr
        )
    {
    }

    FetchUnit(
        logic::Wire& clock,
        logic::Wire& reset,

        logic::Bus<CPUAddressWidth>& pc0,
        logic::Bus<CPUAddressWidth>& pc1,

        logic::Bus<InstructionWidth>& instruction0,
        logic::Bus<InstructionWidth>& instruction1,

        logic::Wire& hit0,
        logic::Wire& hit1,

        logic::Wire& valid0,
        logic::Wire& valid1,

        logic::Bus<CPUAddressWidth>& next_pc,
        logic::Wire& stall
    )
        : FetchUnit(
            clock,
            reset,
            pc0,
            pc1,
            instruction0,
            instruction1,
            hit0,
            hit1,
            valid0,
            valid1,
            next_pc,
            &stall
        )
    {
    }

    void evaluate() noexcept override
    {
        /*
         * Handling optional stall control
         */
        if (stall_wire_ != nullptr)
        {
            pc_enable_.write(
                stall_wire_->read() == logic::LogicState::HIGH
                    ? logic::LogicState::LOW
                    : logic::LogicState::HIGH
            );
        }

        /*
         1. Updating the program counter register
         */
        program_counter_.evaluate();

        /*
         2. Calculating sequential address offsets:
             Lane 1 = PC + 4
            Next   = PC + 8
         */
        pc_plus_4_adder.evaluate();
        pc_plus_8_adder.evaluate();

        /*
          3. Re-evaluate program counter combinational multiplexers so that
             next_pc (PC + 8) is routed to register input for the next clock edge.
         */
        program_counter_.evaluate();

        /*
          4.Driving output address buses
         */
        copy_bus(pc_, pc0_);
        copy_bus(pc_plus_4_, pc1_);
        copy_bus(pc_plus_8_, next_pc_);

        /*
         5. Deriving valid signals
         */
        if (reset_.read() == logic::LogicState::HIGH)
        {
            valid0_.write(logic::LogicState::LOW);
            valid1_.write(logic::LogicState::LOW);
        }
        else
        {
            valid0_.write(hit0_.read());
            valid1_.write(hit1_.read());
        }
    }

    [[nodiscard]]
    logic::Bus<CPUAddressWidth>& pc() noexcept
    {
        return pc_;
    }

    [[nodiscard]]
    const logic::Bus<CPUAddressWidth>& pc() const noexcept
    {
        return pc_;
    }

    [[nodiscard]]
    logic::Bus<CPUAddressWidth>& pc0() noexcept
    {
        return pc0_;
    }

    [[nodiscard]]
    const logic::Bus<CPUAddressWidth>& pc0() const noexcept
    {
        return pc0_;
    }

    [[nodiscard]]
    logic::Bus<CPUAddressWidth>& pc1() noexcept
    {
        return pc1_;
    }

    [[nodiscard]]
    const logic::Bus<CPUAddressWidth>& pc1() const noexcept
    {
        return pc1_;
    }

    [[nodiscard]]
    logic::Bus<CPUAddressWidth>& next_pc() noexcept
    {
        return next_pc_;
    }

    [[nodiscard]]
    const logic::Bus<CPUAddressWidth>& next_pc() const noexcept
    {
        return next_pc_;
    }

    [[nodiscard]]
    logic::Wire& valid0() noexcept
    {
        return valid0_;
    }

    [[nodiscard]]
    const logic::Wire& valid0() const noexcept
    {
        return valid0_;
    }

    [[nodiscard]]
    logic::Wire& valid1() noexcept
    {
        return valid1_;
    }

    [[nodiscard]]
    const logic::Wire& valid1() const noexcept
    {
        return valid1_;
    }

    [[nodiscard]]
    FetchBundle get_bundle() const noexcept
    {
        return FetchBundle(
            static_cast<uint32_t>(bus_to_value(pc0_)),
            static_cast<uint32_t>(bus_to_value(instruction0_)),
            valid0_.read() == logic::LogicState::HIGH,
            static_cast<uint32_t>(bus_to_value(pc1_)),
            static_cast<uint32_t>(bus_to_value(instruction1_)),
            valid1_.read() == logic::LogicState::HIGH
        );
    }

    template <std::size_t Width>
    [[nodiscard]]
    static std::size_t bus_to_value(const logic::Bus<Width>& bus) noexcept
    {
        std::size_t value = 0;
        for (std::size_t i = 0; i < Width; ++i)
        {
            if (bus[i].read() == logic::LogicState::HIGH)
            {
                value |= (1ULL << i);
            }
        }
        return value;
    }

    template <std::size_t Width>
    static void value_to_bus(std::size_t value, logic::Bus<Width>& bus) noexcept
    {
        for (std::size_t i = 0; i < Width; ++i)
        {
            bus[i].write(
                (value & (1ULL << i))
                    ? logic::LogicState::HIGH
                    : logic::LogicState::LOW
            );
        }
    }

    template <std::size_t Width>
    static void copy_bus(const logic::Bus<Width>& source, logic::Bus<Width>& destination) noexcept
    {
        for (std::size_t i = 0; i < Width; ++i)
        {
            destination[i].write(
                source[i].read()
            );
        }
    }

private:

    FetchUnit(
        logic::Wire& clock,
        logic::Wire& reset,

        logic::Bus<CPUAddressWidth>& pc0,
        logic::Bus<CPUAddressWidth>& pc1,

        logic::Bus<InstructionWidth>& instruction0,
        logic::Bus<InstructionWidth>& instruction1,

        logic::Wire& hit0,
        logic::Wire& hit1,

        logic::Wire& valid0,
        logic::Wire& valid1,

        logic::Bus<CPUAddressWidth>& next_pc,
        logic::Wire* stall
    )
        : clock_(clock),
          reset_(reset),

          pc0_(pc0),
          pc1_(pc1),

          instruction0_(instruction0),
          instruction1_(instruction1),

          hit0_(hit0),
          hit1_(hit1),

          valid0_(valid0),
          valid1_(valid1),

          next_pc_(next_pc),

          stall_wire_(stall),

          pc_plus_4_adder(
              pc_,
              instruction_bytes_,
              pc_plus_4_carry_in_,
              pc_plus_4_,
              pc_plus_4_carry_out_
          ),

          pc_plus_8_adder(
              pc_,
              fetch_bytes_,
              pc_plus_8_carry_in_,
              pc_plus_8_,
              pc_plus_8_carry_out_
          ),

          program_counter_(
              clock_,
              reset_,
              pc_enable_,
              pc_plus_8_,
              pc_
          )
    {
        pc_enable_.write(
            logic::LogicState::HIGH
        );

        pc_plus_4_carry_in_.write(
            logic::LogicState::LOW
        );

        pc_plus_8_carry_in_.write(
            logic::LogicState::LOW
        );

        value_to_bus(
            InstructionBytes,
            instruction_bytes_
        );

        value_to_bus(
            FetchBytes,
            fetch_bytes_
        );
    }

    logic::Wire& clock_;
    logic::Wire& reset_;

    logic::Bus<CPUAddressWidth>& pc0_;
    logic::Bus<CPUAddressWidth>& pc1_;

    logic::Bus<InstructionWidth>& instruction0_;
    logic::Bus<InstructionWidth>& instruction1_;

    logic::Wire& hit0_;
    logic::Wire& hit1_;

    logic::Wire& valid0_;
    logic::Wire& valid1_;

    logic::Bus<CPUAddressWidth>& next_pc_;

    logic::Wire* stall_wire_{nullptr};

    /*
     Architectural PC register bus
     */
    logic::Bus<CPUAddressWidth> pc_;

    /*
     Constants
     */
    logic::Bus<CPUAddressWidth> instruction_bytes_;
    logic::Bus<CPUAddressWidth> fetch_bytes_;

    /*
     PC enable control
     */
    logic::Wire pc_enable_;

    /*
     Adder buses & carry wires
     */
    logic::Bus<CPUAddressWidth> pc_plus_4_;
    logic::Bus<CPUAddressWidth> pc_plus_8_;

    logic::Wire pc_plus_4_carry_in_{logic::LogicState::LOW};
    logic::Wire pc_plus_4_carry_out_{logic::LogicState::LOW};
    logic::Wire pc_plus_8_carry_in_{logic::LogicState::LOW};
    logic::Wire pc_plus_8_carry_out_{logic::LogicState::LOW};

    /*
     Sequential ProgramCounter
     */
    ProgramCounter<CPUAddressWidth> program_counter_;

    /*
     Ripple carry adders
     */
    logic::RippleCarryAdder<CPUAddressWidth> pc_plus_4_adder;
    logic::RippleCarryAdder<CPUAddressWidth> pc_plus_8_adder;
};

} // namespace cpu
