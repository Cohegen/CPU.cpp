#include <cassert>
#include <cstdint>

#include "../include/isa/Instruction.hpp"
#include "../include/isa/InstructionDecoder.hpp"

int main()
{
    //testing ADD
    constexpr std::uint32_t raw =
        (0x01u << 26) |   // ADD
        (0x03u << 22) |   // rd = r3
        (0x01u << 18) |   // rs1 = r1
        (0x02u << 14);    // rs2 = r2

    cpu::Instruction instruction{raw};

    cpu::DecodedInstruction decoded =
        cpu::InstructionDecoder::decode(instruction);

    assert(decoded.opcode == cpu::Opcode::ADD);

    assert(
        decoded.format ==
        cpu::InstructionFormat::R_TYPE
    );

    assert(decoded.rd == cpu::Register::R3);
    assert(decoded.rs1 == cpu::Register::R1);
    assert(decoded.rs2 == cpu::Register::R2);

    //testing immediate
    constexpr std::uint32_t raw1 =
        (0x08u << 26) |   // ADDI
        (0x03u << 22) |   // rd = r3
        (0x01u << 18) |   // rs1 = r1
        10u;              // immediate = 10

    cpu::Instruction instruction1{raw1};

    auto decoded1 =
        cpu::InstructionDecoder::decode(instruction1);

    assert(decoded1.opcode == cpu::Opcode::ADDI);
    assert(decoded1.format == cpu::InstructionFormat::I_TYPE);
    assert(decoded1.rd == cpu::Register::R3);
    assert(decoded1.rs1 == cpu::Register::R1);
    assert(decoded1.immediate == 10);

    //testing negative immediate
    constexpr std::uint32_t raw2 =
        (0x08u << 26) |
        (0x03u << 22) |
        (0x01u << 18) |
        0x3FFFBu;        // 18-bit representation of -5

    cpu::Instruction instruction2{raw2};
    auto decoded2 = cpu::InstructionDecoder::decode(instruction2);

    assert(decoded2.immediate == -5);
    return 0;
}