#include "../include/isa/InstructionDecoder.hpp"

namespace cpu {

DecodedInstruction InstructionDecoder::decode(
    const Instruction& instruction
) {
    DecodedInstruction decoded{};

    decoded.opcode = instruction.opcode();
    decoded.rd = instruction.rd();
    decoded.rs1 = instruction.rs1();
    decoded.rs2 = instruction.rs2();
    decoded.immediate = instruction.immediate_signed();

    switch (decoded.opcode) {

        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::XOR:
            decoded.format = InstructionFormat::R_TYPE;
            break;

        case Opcode::NOT:
            decoded.format = InstructionFormat::R_TYPE;
            break;

        case Opcode::LI:
        case Opcode::ADDI:
        case Opcode::LW:
            decoded.format = InstructionFormat::I_TYPE;
            break;

        case Opcode::SW:
            decoded.format = InstructionFormat::S_TYPE;
            break;

        case Opcode::BEQ:
        case Opcode::BNE:
            decoded.format = InstructionFormat::B_TYPE;
            break;

        case Opcode::J:
            decoded.format = InstructionFormat::J_TYPE;
            break;

        case Opcode::NOP:
        case Opcode::HALT:
            decoded.format = InstructionFormat::I_TYPE;
            break;
    }

    return decoded;
}

}