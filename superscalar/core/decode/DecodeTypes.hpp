#pragma once

namespace cpu
{
    enum class ALUOperation{
        NONE,
        ADD,
        SUB,
        AND,
        OR,
        XOR,
        NOT
    };

    enum class OperandSource
    {
        NONE,
        REGISTER,
        IMMEDIATE,
        PC
    };

    enum class ControlFlow
    {
        NONE,
        BRANCH,
        JUMP
    };
}