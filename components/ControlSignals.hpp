#pragma once
#include <cstdint>

namespace cpu {

enum class ALUOperation : std::uint8_t
{
    ADD,
    SUB,
    AND,
    OR,
    XOR,
    NOT,
    PASS_A,
    PASS_B
};


struct ControlSignals {
    // register file
    bool register_write = false;

    // ALU
    bool alu_source_immediate = false;
    ALUOperation alu_operation = ALUOperation::ADD;

    // Data memory
    bool memory_read = false;
    bool memory_write = false;

    // Control flow
    bool branch = false;
    bool jump = false;

    // Processor state
    bool halt = false;
};

}