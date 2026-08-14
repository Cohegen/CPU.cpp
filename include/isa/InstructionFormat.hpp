/*
There as several types of instructions namely:
    - R-type(both source operands are registers)
    - I-type (one source operand is an immediate value)
    -Branch
    - Jump
*/

#pragma once

#include <cstdint>

namespace cpu{
    enum class InstructionFormat: std::uint8_t{
        R_TYPE,
        I_TYPE,
        S_TYPE,
        B_TYPE
        J_TYPE,
    };
}