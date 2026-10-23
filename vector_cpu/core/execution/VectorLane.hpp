/*
Implementation of a vector Lane
It's responsible for performing any operation on one 
element of a vector
*/
#pragma once

#include <cstddef>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>
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
            logic::Bus<2>& opcode,
            logic::Bus<32>& result,
            logic::Wire& alu_zero,
            logic::Wire& alu_carry
        ) :
            operand_a_(operand_a),
            operand_b_(operand_b),
            opcode_2_(&opcode),
            opcode_3_(nullptr),
            result_(result),
            alu_zero_(alu_zero),
            alu_carry_(alu_carry),
            alu_(operand_a_, operand_b_, alu_opcode_, result_, alu_zero_, alu_carry_)
        {
            update_alu_opcode();
        }

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
            opcode_2_(nullptr),
            opcode_3_(&opcode),
            result_(result),
            alu_zero_(alu_zero),
            alu_carry_(alu_carry),
            alu_(operand_a_, operand_b_, alu_opcode_, result_, alu_zero_, alu_carry_)
        {
            update_alu_opcode();
        }

        void evaluate() noexcept override
        {
            update_alu_opcode();
            alu_.evaluate();
        }

        [[nodiscard]] logic::Wire& zero() noexcept { return alu_zero_; }
        [[nodiscard]] const logic::Wire& zero() const noexcept { return alu_zero_; }
        [[nodiscard]] logic::Wire& carry() noexcept { return alu_carry_; }
        [[nodiscard]] const logic::Wire& carry() const noexcept { return alu_carry_; }

        [[nodiscard]] logic::Bus<32>& operand_a() noexcept { return operand_a_; }
        [[nodiscard]] logic::Bus<32>& operand_b() noexcept { return operand_b_; }
        [[nodiscard]] logic::Bus<32>& result() noexcept { return result_; }

        // Status outputs exposed as references
        logic::Wire& alu_zero_;
        logic::Wire& alu_carry_;

    private:
        void update_alu_opcode() noexcept
        {
            if (opcode_2_ != nullptr)
            {
                alu_opcode_[0].write((*opcode_2_)[0].read());
                alu_opcode_[1].write((*opcode_2_)[1].read());
                alu_opcode_[2].write(logic::LogicState::LOW);
            }
            else if (opcode_3_ != nullptr)
            {
                alu_opcode_[0].write((*opcode_3_)[0].read());
                alu_opcode_[1].write((*opcode_3_)[1].read());
                alu_opcode_[2].write((*opcode_3_)[2].read());
            }
        }

        // inputs
        logic::Bus<32>& operand_a_;
        logic::Bus<32>& operand_b_;
        logic::Bus<2>* opcode_2_{nullptr};
        logic::Bus<3>* opcode_3_{nullptr};

        // outputs
        logic::Bus<32>& result_;

        // internal signals and hardware components
        logic::Bus<3> alu_opcode_;
        logic::ALU<32> alu_;
    };
}
