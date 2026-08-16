/*
ALUOperandMux selects the second operand supplied to the ALU
 
Operand sources :
               alu_source_immediate = LOW
                  → register operand (rs2)

             alu_source_immediate = HIGH
               → immediate operand
   
*/

#pragma once

#include <cstddef>

#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu
{

template <std::size_t N = 32>
class ALUOperandMux : public logic::Component
{
public:
    ALUOperandMux(
        logic::Bus<N>& register_operand,
        logic::Bus<N>& immediate_operand,
        logic::Wire& select,
        logic::Bus<N>& output
    )
        : register_operand_(register_operand),
          immediate_operand_(immediate_operand),
          select_(select),
          output_(output),
          mux_(
              register_operand_,
              immediate_operand_,
              select_,
              output_)
    {
    }

    void evaluate() noexcept override
    {
        mux_.evaluate();
    }

private:
    logic::Bus<N>& register_operand_;
    logic::Bus<N>& immediate_operand_;
    logic::Wire& select_;
    logic::Bus<N>& output_;

    logic::Mux<N> mux_;
};

} // namespace cpu

