/*
  Combinational Control Unit for the Pipelined CPU.
    Translates decoded ISA instructions into PipelinedControlSignals.
*/

#pragma once

#include "../../include/isa/DecodedInstruction.hpp"
#include "../../components/ControlSignals.hpp"
#include "ControlSignals.hpp"

namespace cpu {

class PipelinedControlUnit {
public:
    [[nodiscard]]
    static PipelinedControlSignals generate(const DecodedInstruction& instruction) noexcept {
        PipelinedControlSignals signals{};

        switch (instruction.opcode) {
            case Opcode::NOP:
                break;

            // R-type arithmetic
            case Opcode::ADD:
                signals.regDst = true;
                signals.aluSrc = false;
                signals.aluOp = ALUOperation::ADD;
                signals.regWrite = true;
                break;

            case Opcode::SUB:
                signals.regDst = true;
                signals.aluSrc = false;
                signals.aluOp = ALUOperation::SUB;
                signals.regWrite = true;
                break;

            // Logical operations
            case Opcode::AND:
                signals.regDst = true;
                signals.aluSrc = false;
                signals.aluOp = ALUOperation::AND;
                signals.regWrite = true;
                break;

            case Opcode::OR:
                signals.regDst = true;
                signals.aluSrc = false;
                signals.aluOp = ALUOperation::OR;
                signals.regWrite = true;
                break;

            case Opcode::XOR:
                signals.regDst = true;
                signals.aluSrc = false;
                signals.aluOp = ALUOperation::XOR;
                signals.regWrite = true;
                break;

            case Opcode::NOT:
                signals.regDst = true;
                signals.aluSrc = false;
                signals.aluOp = ALUOperation::NOT;
                signals.regWrite = true;
                break;

            // Immediate operations
            case Opcode::LI:
                signals.regDst = true;
                signals.aluSrc = true;
                signals.aluOp = ALUOperation::PASS_B;
                signals.regWrite = true;
                break;

            case Opcode::ADDI:
                signals.regDst = true;
                signals.aluSrc = true;
                signals.aluOp = ALUOperation::ADD;
                signals.regWrite = true;
                break;

            // Memory operations
            case Opcode::LW:
                signals.regDst = true;
                signals.aluSrc = true;
                signals.aluOp = ALUOperation::ADD;
                signals.memRead = true;
                signals.regWrite = true;
                signals.memToReg = true;
                break;

            case Opcode::SW:
                signals.regDst = false;
                signals.aluSrc = true;
                signals.aluOp = ALUOperation::ADD;
                signals.memWrite = true;
                signals.regWrite = false;
                break;

            // Conditional branches
            case Opcode::BEQ:
            case Opcode::BNE:
                signals.regDst = false;
                signals.aluSrc = false;
                signals.aluOp = ALUOperation::SUB;
                signals.branch = true;
                signals.regWrite = false;
                break;

            // Unconditional jump
            case Opcode::J:
                signals.regDst = false;
                signals.aluSrc = false;
                signals.branch = true;
                signals.regWrite = false;
                break;

            // Halt
            case Opcode::HALT:
                signals.regDst = false;
                signals.regWrite = false;
                break;
        }

        return signals;
    }
};

} // namespace cpu
