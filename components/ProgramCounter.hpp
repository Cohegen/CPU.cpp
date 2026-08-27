/*
    ProgramCounter.hpp

    An N-bit Program Counter (PC) component for CPU datapaths.

    Built using:
        - Register<N> (N-bit sequential D flip-flop register)
        - Mux<N>      (N-bit control multiplexers)

    Control logic:
        - reset = 1: PC loads zero.
        - reset = 0, enable = 1: PC loads next.
        - reset = 0, enable = 0: PC holds its current value.
*/

#pragma once

#include <cstddef>

#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu
{

template <std::size_t N = 32>
class ProgramCounter : public logic::Component
{
public:

    ProgramCounter(
        logic::Wire& clock,
        logic::Wire& reset,
        logic::Wire& enable,
        logic::Bus<N>& next,
        logic::Bus<N>& output
    )
        : enable_mux_(
            output,
            next,
            enable,
            enable_mux_out_
        ),
          reset_mux_(
            enable_mux_out_,
            zero_bus_,
            reset,
            register_input_
        ),
          pc_reg_(
            register_input_,
            clock,
            output
        ),
          next_(next),
          output_(output)
    {
        zero_bus_.write(logic::LogicState::LOW);
    }

    void evaluate() noexcept override
    {
        enable_mux_.evaluate();
        reset_mux_.evaluate();
        pc_reg_.evaluate();
    }

    [[nodiscard]]
    logic::Bus<N>& output() noexcept
    {
        return output_;
    }

    [[nodiscard]]
    const logic::Bus<N>& output() const noexcept
    {
        return output_;
    }

private:

    // External input
    logic::Bus<N>& next_;

    // External output
    logic::Bus<N>& output_;

    // Internal buses
    logic::Bus<N> zero_bus_;
    logic::Bus<N> enable_mux_out_;
    logic::Bus<N> register_input_;

    // Hardware
    logic::Mux<N> enable_mux_;
    logic::Mux<N> reset_mux_;
    logic::Register<N> pc_reg_;
};

}