#include "../include/isa/InstructionDecoder.hpp"

namespace cpu {

DecodedInstruction InstructionDecoder::decode(
    const Instruction& instruction
) {
    DecodedInstruction decoded{};

    decoded.opcode = instruction.opcode();
    decoded.rd = Register::R0;
    decoded.rs1 = Register::R0;
    decoded.rs2 = Register::R0;
    decoded.immediate = 0;

    switch (decoded.opcode) {

        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::AND:
        case Opcode::OR:
        case Opcode::XOR:
        case Opcode::NOT:
            decoded.format = InstructionFormat::R_TYPE;
            break;

        case Opcode::LI:
        case Opcode::ADDI:
        case Opcode::LW:
        case Opcode::NOP:
        case Opcode::HALT:
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
    }

    switch (decoded.format) {
        case InstructionFormat::R_TYPE:
            // R-type: [opcode 6][rd 4][rs1 4][rs2 4][unused 14]
            decoded.rd = instruction.rd();
            decoded.rs1 = instruction.rs1();
            decoded.rs2 = instruction.rs2();
            break;

        case InstructionFormat::I_TYPE:
            // I-type: [opcode 6][rd 4][rs1 4][immediate 18]
            decoded.rd = instruction.rd();
            decoded.rs1 = instruction.rs1();
            decoded.immediate = instruction.immediate();
            break;

        case InstructionFormat::S_TYPE:
            // S-type: [opcode 6][rs2 4][rs1 4][immediate 18]
            decoded.rs2 = instruction.rd(); // bits 25..22
            decoded.rs1 = instruction.rs1(); // bits 21..18
            decoded.immediate = instruction.immediate();
            break;

        case InstructionFormat::B_TYPE:
            // B-type: [opcode 6][rs1 4][rs2 4][immediate 18]
            decoded.rs1 = instruction.rd(); // bits 25..22
            decoded.rs2 = instruction.rs1(); // bits 21..18
            decoded.immediate = instruction.immediate();
            break;

        case InstructionFormat::J_TYPE:
            // J-type: [opcode 6][immediate 26] or [unused]
            decoded.immediate = instruction.immediate();
            break;
    }

    return decoded;
}

}