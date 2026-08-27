/*
An implementation of the ControlUnit

It's a Combinational control unit for the CPU
It converts a decoded instruction into the control signals
required by the single-cycle datapath
*/
#pragma once

#include "../../components/ControlSignals.hpp"
#include "../../include/isa/DecodedInstruction.hpp"


namespace cpu {

class ControlUnit {
public:
    [[nodiscard]]
    static ControlSignals generate(const DecodedInstruction& instruction) noexcept {
        ControlSignals signals{};

        switch (instruction.opcode) {
            case Opcode::NOP:
                break;

            // R-type arithmetic
            case Opcode::ADD:
                signals.register_write = true;
                signals.alu_operation = ALUOperation::ADD;
                break;

            case Opcode::SUB:
                signals.register_write = true;
                signals.alu_operation = ALUOperation::SUB;
                break;

            // Logical operations
            case Opcode::AND:
                signals.register_write = true;
                signals.alu_operation = ALUOperation::AND;
                break;

            case Opcode::OR:
                signals.register_write = true;
                signals.alu_operation = ALUOperation::OR;
                break;

            case Opcode::XOR:
                signals.register_write = true;
                signals.alu_operation = ALUOperation::XOR;
                break;

            case Opcode::NOT:
                signals.register_write = true;
                signals.alu_operation = ALUOperation::NOT;
                break;

            // Immediate operations
            case Opcode::LI:
                signals.register_write = true;
                signals.alu_source_immediate = true;
                signals.alu_operation = ALUOperation::PASS_B;
                break;

            case Opcode::ADDI:
                signals.register_write = true;
                signals.alu_source_immediate = true;
                signals.alu_operation = ALUOperation::ADD;
                break;

            // Memory operations
            case Opcode::LW:
                signals.register_write = true;
                signals.alu_source_immediate = true;
                signals.alu_operation = ALUOperation::ADD;
                signals.memory_read = true;
                break;

            case Opcode::SW:
                signals.alu_source_immediate = true;
                signals.alu_operation = ALUOperation::ADD;
                signals.memory_write = true;
                break;

            // Conditional branches
            case Opcode::BEQ:
            case Opcode::BNE:
                signals.alu_operation = ALUOperation::SUB;
                signals.branch = true;
                break;

            // Unconditional jump
            case Opcode::J:
                signals.jump = true;
                break;

            // Processor state
            case Opcode::HALT:
                signals.halt = true;
                break;
        }

        return signals;
    }
};

} // namespace cpu