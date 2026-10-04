#pragma once


#include "../../../components/ControlSignals.hpp"

namespace cpu
{
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
