/*
A cpu-lvel interface to the ALU implemented in Logic.cpp


*/

#pragma once

#include <cstddef>

#include <logic/combinational/alu/ALU.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>

#include "ControlSignals.hpp"

namespace cpu{
    template<std::size_t N=32>
    class ALUInterface: public logic::Component
    {
        public:
        ALUInterface(
            logic::Bus<N>& a,
            logic::Bus<N>& b,
            ALUOperation operation,
            logic::Bus<N>& result,
            logic::Wire& zero,
            logic::Wire& carry
        )
            : a_(a),
              b_(b),
              operation_(operation),
              result_(result),
              zero_(zero),
              carry_(carry),
              alu_(a_, b_, opcode_, result_, zero_, carry_)
        {
            update_opcode();
        }
    
        void set_operation(ALUOperation operation) noexcept
        {
            operation_ = operation;
        }
    
        [[nodiscard]]
        ALUOperation operation() const noexcept
        {
            return operation_;
        }
    
        void evaluate() noexcept override
        {
            update_opcode();
            alu_.evaluate();
        }

        private:
        logic::Bus<N>& a_;
        logic::Bus<N>& b_;

        ALUOperation operation_;

        logic::Bus<N>& result_;
        logic::Wire& zero_;
        logic::Wire& carry_;

        logic::Bus<3> opcode_;

        logic::ALU<N> alu_;

        void update_opcode() noexcept{
            const std::uint8_t opcode = encode_operation(operation_);

            
            opcode_.write_value(opcode);
        }

        [[nodiscard]]
        static constexpr std::uint8_t encode_operation(ALUOperation operation) noexcept{
            switch (operation)
        {
            case ALUOperation::ADD:
                return 0b000;

            case ALUOperation::SUB:
                return 0b001;

            case ALUOperation::AND:
                return 0b010;

            case ALUOperation::OR:
                return 0b011;

            case ALUOperation::XOR:
                return 0b100;

            case ALUOperation::NOT:
                return 0b101;

            case ALUOperation::PASS_A:
                return 0b110;

            case ALUOperation::PASS_B:
                return 0b111;
        }

        return 0b000;
        }

    };
}