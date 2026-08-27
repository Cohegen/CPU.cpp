/*
    

    CPU instruction memory implemented using the ROM primitive
    provided by Logic.cpp.

    Address:
        CPU program-counter address.

    Output:
        Instruction word stored at the corresponding ROM address.

    Address mapping:
        The lower ROMAddressWidth bits of the CPU address
        are connected to the ROM address bus.

    Example:
        InstructionMemory<32, 8, 32>

        CPU address      : 32 bits
        ROM address      : 8 bits
        Instruction width: 32 bits

        ROM capacity:
            2^8 = 256 instructions
*/

#pragma once

#include <cstddef>
#include <vector>

#include <logic/sequential/memory/ROM.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu
{

template <
    std::size_t CPUAddressWidth = 32,
    std::size_t ROMAddressWidth = 8,
    std::size_t InstructionWidth = 32
>
class InstructionMemory : public logic::Component
{
    static_assert(
        CPUAddressWidth > 0,
        "CPU address width must be greater than zero."
    );

    static_assert(
        ROMAddressWidth > 0,
        "ROM address width must be greater than zero."
    );

    static_assert(
        InstructionWidth > 0,
        "Instruction width must be greater than zero."
    );

    static_assert(
        ROMAddressWidth <= CPUAddressWidth,
        "ROM address width cannot exceed CPU address width."
    );

public:

    /*
        Constructor for an always-enabled instruction memory.

        The instruction memory owns its enable signal and keeps it HIGH.
    */
    InstructionMemory(
        logic::Bus<CPUAddressWidth>& address,
        logic::Bus<InstructionWidth>& instruction,
        const std::vector<std::size_t>& contents = {}
    )
        : address_(address),
          instruction_(instruction),
          owned_enable_(logic::LogicState::HIGH),
          enable_ref_(owned_enable_),
          rom_address_bus_{},
          rom_(
              enable_ref_,
              rom_address_bus_,
              instruction_,
              contents
          )
    {
    }

    /*
        Constructor allowing the instruction memory enable signal
        to be controlled externally.
    */
    InstructionMemory(
        logic::Wire& enable,
        logic::Bus<CPUAddressWidth>& address,
        logic::Bus<InstructionWidth>& instruction,
        const std::vector<std::size_t>& contents = {}
    )
        : address_(address),
          instruction_(instruction),
          owned_enable_(logic::LogicState::HIGH),
          enable_ref_(enable),
          rom_address_bus_{},
          rom_(
              enable_ref_,
              rom_address_bus_,
              instruction_,
              contents
          )
    {
    }

    /*
        Evaluates the instruction-memory datapath.

        CPU address:
            address_

        ROM address:
            rom_address_bus_

        Instruction:
            instruction_
    */
    void evaluate() noexcept override
    {
        /*
            Map the lower ROMAddressWidth bits of the
            CPU address onto the ROM address bus.
        */
        for (std::size_t i = 0; i < ROMAddressWidth; ++i)
        {
            rom_address_bus_[i].write(
                address_[i].read()
            );
        }

        /*
            Evaluate the underlying ROM.
        */
        rom_.evaluate();
    }

    /*
        Replace the complete ROM contents.
    */
    void load(
        const std::vector<std::size_t>& contents
    ) noexcept
    {
        rom_.load_contents(contents);
    }

    /*
        Write a single instruction directly into ROM.

        Useful for tests and small programs.
    */
    void set_word(
        std::size_t index,
        std::size_t value
    ) noexcept
    {
        rom_.set_word(index, value);
    }

    /*
        Read a single instruction directly from ROM.

        This is primarily a testing/debugging interface.
    */
    [[nodiscard]]
    std::size_t get_word(
        std::size_t index
    ) const noexcept
    {
        return rom_.get_word(index);
    }

private:

   
    // External CPU interface
    logic::Bus<CPUAddressWidth>& address_;
    logic::Bus<InstructionWidth>& instruction_;

   
    // Enable signal

    /*
        Used when the instruction memory is constructed without
        an external enable signal.
    */
    logic::Wire owned_enable_;

    /*
        Reference used by the ROM.

        It either refers to owned_enable_ or to the externally
        supplied enable signal.
    */
    logic::Wire& enable_ref_;

   
    // Internal ROM address bus

    logic::Bus<ROMAddressWidth> rom_address_bus_;

    
    // Hardware
    logic::ROM<
        ROMAddressWidth,
        InstructionWidth
    > rom_;
};

} 