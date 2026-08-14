
#pragma once

#include <cstdint>

#include "InstructionFormat.hpp"
#include "Opcode.hpp"
#include "Registers.hpp"

namespace cpu{

    struct DecodedInstruction{
        Opcode opcode;
        InstructionFormat format;

        Register rd;
        Register rs1;
        Register rs2;

        std::int32_t immediate;
    };
}