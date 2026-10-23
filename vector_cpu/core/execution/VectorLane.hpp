/*
Implementation of a vector Lane
It's responsible for performing any operation on one 
element of a vector
*/
#pragma once

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/combinational/alu/ALU.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu
{
    class VectorLane : public logic::Component
    {
    public:
        VectorLane(
            logic::Bus<32>& operand_a,
            logic::Bus<32>& operand_b,
            logic::Bus<3>& opcode,
            logic::Bus<32>& result,
            logic::Wire& alu_zero,
            logic::Wire& alu_carry
        ) :
            operand_a_(operand_a),
            operand_b_(operand_b),
            opcode_(opcode),
            result_(result),
            alu_zero_(alu_zero),
            alu_carry_(alu_carry),
            alu_(operand_a_, operand_b_, opcode_, result_, alu_zero_, alu_carry_)
        {}

        void evaluate() noexcept override
        {
            alu_.evaluate();
        }

    private:
        // inputs
        logic::Bus<32>& operand_a_;
        logic::Bus<32>& operand_b_;
        logic::Bus<3>& opcode_;

        // outputs
        logic::Bus<32>& result_;
        logic::Wire& alu_zero_;
        logic::Wire& alu_carry_;

        // internal hardware components
        logic::ALU<32> alu_;
    };
}
