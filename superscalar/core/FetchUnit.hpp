#pragma once

#include <cstddef>

#include <logic/combinational/adders/RippleCarryAdder.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

#include "ProgramCounter.hpp"

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

          pc_plus_4_adder(
              pc_,
              instruction_bytes_,
              pc_plus_4_
          ),

          pc_plus_8_adder(
              pc_,
              fetch_bytes_,
              pc_plus_8_
          ),

          program_counter_(
              clock_,
              reset_,
              pc_enable_,
              pc_plus_8_,
              pc_
          )
    {
        /*
         * The PC should advance every cycle in this
         * initial sequential-only implementation
         */
        pc_enable_.write(
            logic::LogicState::HIGH
        );

        /*
         * Constants:

              InstructionBytes = 4
              FetchBytes       = 8

         */
        value_to_bus(
            InstructionBytes,
            instruction_bytes_
        );

        value_to_bus(
            FetchBytes,
            fetch_bytes_
        );
    }


    void evaluate() noexcept override
    {
        /*
         updating pc
         */
        program_counter_.evaluate();


        /*
          Calculate:

              PC + 4
              PC + 8
         */
        pc_plus_4_.evaluate();
        pc_plus_8_.evaluate();


        /*
         Lane 0 fetches from PC.

         Lane 1 fetches from PC + 4.
         */
        copy_bus(pc_,pc0_);

        copy_bus(pc_plus_4_,pc1_);


        /*
          For the initial fetch stage, an instruction is
         valid whenever its cache lookup hits
         */
        valid0_.write(hit0_.read());

        valid1_.write(hit1_.read());


        /*
         The instruction buses themselves are supplied
         by the instruction cache
         FetchUnit therefore does not copy or modify them
         */
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


private:

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


    /*
      Internal PC.

     This is the architectural fetch PC
     */
    logic::Bus<CPUAddressWidth> pc_;


    /*
     Constants represented as Logic.cpp buses.
     */
    logic::Bus<CPUAddressWidth> instruction_bytes_;
    logic::Bus<CPUAddressWidth> fetch_bytes_;


    /*
     Sequential PC control
     */
    logic::Wire pc_enable_;


    /*
     Sequential PC + 4
     */
    logic::Bus<CPUAddressWidth> pc_plus_4_;


    /*
     Sequential PC + 8
     */
    logic::Bus<CPUAddressWidth> pc_plus_8_;



    ProgramCounter<CPUAddressWidth> program_counter_;


    /*
     Address arithmetic
     */
    logic::RippleCarryAdder<CPUAddressWidth> pc_plus_4_adder;
    logic::RippleCarryAdder<CPUAddressWidth> pc_plus_8_adder;


    template <std::size_t Width>
    static void value_to_bus(std::size_t value,logic::Bus<Width>& bus) noexcept
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
    static void copy_bus(const logic::Bus<Width>& source, logic::Bus<Width>& destination ) noexcept
    {
        for (std::size_t i = 0; i < Width; ++i)
        {
            destination[i].write(
                source[i].read()
            );
        }
    }
};

} // namespace cpu
