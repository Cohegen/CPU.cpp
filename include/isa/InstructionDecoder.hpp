/*
Decodes the opcode
*/

#pragma once

#include "DecodedInstruction.hpp"
#include "Instruction.hpp"

namespace cpu{

    class InstructionDecoder{
        public:
        [[nodiscard]]
        static DecodedInstruction decode(
            const Instruction& instruction
        ) noexcept {
            DecodedInstruction decoded{};
            decoded.opcode = instruction.opcode();
            decoded.rd = Register::R0;
            decoded.rs1 = Register::R0;
            decoded.rs2 = Register::R0;
            decoded.immediate = 0;

            switch (decoded.opcode) {
                case Opcode::ADD: case Opcode::SUB: case Opcode::AND:
                case Opcode::OR:  case Opcode::XOR: case Opcode::NOT:
                    decoded.format = InstructionFormat::R_TYPE; break;
                case Opcode::LI: case Opcode::ADDI: case Opcode::LW:
                case Opcode::NOP: case Opcode::HALT:
                    decoded.format = InstructionFormat::I_TYPE; break;
                case Opcode::SW: decoded.format = InstructionFormat::S_TYPE; break;
                case Opcode::BEQ: case Opcode::BNE:
                    decoded.format = InstructionFormat::B_TYPE; break;
                case Opcode::J: decoded.format = InstructionFormat::J_TYPE; break;
            }

            switch (decoded.format) {
                case InstructionFormat::R_TYPE:
                    decoded.rd = instruction.rd(); decoded.rs1 = instruction.rs1(); decoded.rs2 = instruction.rs2(); break;
                case InstructionFormat::I_TYPE:
                    decoded.rd = instruction.rd(); decoded.rs1 = instruction.rs1(); decoded.immediate = instruction.immediate(); break;
                case InstructionFormat::S_TYPE:
                    decoded.rs2 = instruction.rd(); decoded.rs1 = instruction.rs1(); decoded.immediate = instruction.immediate(); break;
                case InstructionFormat::B_TYPE:
                    decoded.rs1 = instruction.rd(); decoded.rs2 = instruction.rs1(); decoded.immediate = instruction.immediate(); break;
                case InstructionFormat::J_TYPE: {
                    std::uint32_t immediate = instruction.raw() & 0x03FFFFFFU;
                    if ((immediate & 0x02000000U) != 0) immediate |= ~0x03FFFFFFU;
                    decoded.immediate = static_cast<std::int32_t>(immediate);
                    break;
                }
            }
            return decoded;
        }
    };
}
