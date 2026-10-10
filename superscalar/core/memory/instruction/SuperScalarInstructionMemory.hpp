#pragma once

#include <cstddef>
#include <vector>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/sequential/memory/DualPortROM.hpp>

namespace cpu
{

template <
    std::size_t CPUAddressWidth = 32,
    std::size_t ROMAddressWidth = 8,
    std::size_t InstructionWidth = 32
>
class SuperscalarInstructionMemory : public logic::Component
{
    static_assert(CPUAddressWidth > 0,
                  "CPUAddressWidth must be greater than zero.");

    static_assert(ROMAddressWidth > 0,
                  "ROMAddressWidth must be greater than zero.");

    static_assert(InstructionWidth > 0,
                  "InstructionWidth must be greater than zero.");

    static_assert(InstructionWidth % 8 == 0,
                  "InstructionWidth must be a multiple of 8.");

public:

    static constexpr std::size_t InstructionBytes =
        InstructionWidth / 8;

    static constexpr std::size_t AddressShift =
        []()
        {
            std::size_t value = InstructionBytes;
            std::size_t bits = 0;

            while (value > 1)
            {
                value >>= 1;
                ++bits;
            }

            return bits;
        }();


    SuperscalarInstructionMemory(
        logic::Wire& request0,
        logic::Wire& request1,

        logic::Bus<CPUAddressWidth>& address0,
        logic::Bus<CPUAddressWidth>& address1,

        logic::Bus<InstructionWidth>& instruction0,
        logic::Bus<InstructionWidth>& instruction1,

        logic::Wire& response0,
        logic::Wire& response1,

        const std::vector<std::size_t>& contents = {}
    )
        : request0_(request0),
          request1_(request1),

          address0_(address0),
          address1_(address1),

          instruction0_(instruction0),
          instruction1_(instruction1),

          response0_(response0),
          response1_(response1),

          rom_address0_{},
          rom_address1_{},

          rom_(
              owned_enable_,
              rom_address0_,
              rom_address1_,
              instruction0_,
              instruction1_,
              contents
          )
    {
    }


    void evaluate() noexcept override
    {
        response0_.write(logic::LogicState::LOW);
        response1_.write(logic::LogicState::LOW);

        clear_output(instruction0_);
        clear_output(instruction1_);


        /*
            Port 0
        */

        if (request0_.read() == logic::LogicState::HIGH)
        {
            convert_address(
                address0_,
                rom_address0_
            );

            response0_.write(logic::LogicState::HIGH);
        }


        /*
            Port 1
        */

        if (request1_.read() == logic::LogicState::HIGH)
        {
            convert_address(
                address1_,
                rom_address1_
            );

            response1_.write(logic::LogicState::HIGH);
        }


        /*
            Enabling the underlying ROM only if at least
            one request is active
        */

        const bool request_active =
            request0_.read() == logic::LogicState::HIGH ||
            request1_.read() == logic::LogicState::HIGH;

        owned_enable_.write(
            request_active
                ? logic::LogicState::HIGH
                : logic::LogicState::LOW
        );


        rom_.evaluate();
    }


    void load(
        const std::vector<std::size_t>& contents
    ) noexcept
    {
        rom_.load_contents(contents);
    }


    void set_word(
        std::size_t index,
        std::size_t value
    ) noexcept
    {
        rom_.set_word(index, value);
    }


    [[nodiscard]]
    std::size_t get_word(
        std::size_t index
    ) const noexcept
    {
        return rom_.get_word(index);
    }


private:

    /*
        Request / response interface
    */

    logic::Wire& request0_;
    logic::Wire& request1_;

    logic::Bus<CPUAddressWidth>& address0_;
    logic::Bus<CPUAddressWidth>& address1_;

    logic::Bus<InstructionWidth>& instruction0_;
    logic::Bus<InstructionWidth>& instruction1_;

    logic::Wire& response0_;
    logic::Wire& response1_;


    /*
        ROM address buses
    */

    logic::Bus<ROMAddressWidth> rom_address0_;
    logic::Bus<ROMAddressWidth> rom_address1_;


    /*
        ROM enable

        DualPortROM has one shared enable for both
        read ports.
    */

    logic::Wire owned_enable_;


    /*
        Underlying dual-port memory.
    */

    logic::DualPortROM<ROMAddressWidth,InstructionWidth> rom_;


    /*
        Converting a CPU byte address into an instruction
        word address

    */

    static void convert_address(
        const logic::Bus<CPUAddressWidth>& cpu_address,
        logic::Bus<ROMAddressWidth>& rom_address
    ) noexcept
    {
        for (std::size_t i = 0; i < ROMAddressWidth; ++i)
        {
            const std::size_t cpu_bit =
                i + AddressShift;

            if (cpu_bit < CPUAddressWidth)
            {
                rom_address[i].write(
                    cpu_address[cpu_bit].read()
                );
            }
            else
            {
                rom_address[i].write(
                    logic::LogicState::LOW
                );
            }
        }
    }


    static void clear_output(
        logic::Bus<InstructionWidth>& output
    ) noexcept
    {
        for (std::size_t i = 0; i < InstructionWidth; ++i)
        {
            output[i].write(
                logic::LogicState::LOW
            );
        }
    }
};

template <
    std::size_t CPUAddressWidth = 32,
    std::size_t ROMAddressWidth = 8,
    std::size_t InstructionWidth = 32
>
using SuperScalarInstructionMemory =
    SuperscalarInstructionMemory<
        CPUAddressWidth,
        ROMAddressWidth,
        InstructionWidth
    >;

} // namespace cpu
