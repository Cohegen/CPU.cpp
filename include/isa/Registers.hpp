/*
Defining our CPU registers
*/

#pragma once
#include <cstddef>
#include <cstdint>

namespace cpu {
    enum class Register : std::uint8_t{
        R0=0,
        R1=1,
        R2=2,
        R3=3,
        R4=4,
        R5=5,
        R6=6,
        R7=7,
        R8=8,
        R9=9,
        R10 =10,
        R11=11,
        R12=12,
        R13=13,
        R14=14,
        R15=15
    };

    constexpr std::size_t REGISTER_COUNT = 16;
    constexpr std::size_t REGISTER_WIDTH = 32;
}