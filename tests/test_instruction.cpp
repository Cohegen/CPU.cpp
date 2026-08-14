#include <cassert>
#include <cstdint>

#include "../include/isa/Instruction.hpp"

int main()
{
    constexpr std::uint32_t raw =
        (0x01u << 26) |   // ADD
        (0x03u << 22) |   // rd = r3
        (0x01u << 18) |   // rs1 = r1
        (0x02u << 14);    // rs2 = r2

    cpu::Instruction instruction{raw};

    assert(
        instruction.opcode() == cpu::Opcode::ADD
    );

    assert(
        instruction.rd() == cpu::Register::R3
    );

    assert(
        instruction.rs1() == cpu::Register::R1
    );

    assert(
        instruction.rs2() == cpu::Register::R2
    );

    return 0;
}