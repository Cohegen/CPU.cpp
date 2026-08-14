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
        );
    };
}