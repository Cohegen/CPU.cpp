#pragma once

#include <cstddef>
#include <cstdint>

#if __has_include("isa/Opcode.hpp")
#include "isa/Opcode.hpp"
#elif __has_include("Opcode.hpp")
#include "Opcode.hpp"
#else
#include "../../../include/isa/Opcode.hpp"
#endif

#include "../rename/RenameBundle.hpp"
#include "../decode/DecodeTypes.hpp"

namespace cpu {
    struct ExecutionRequest
    {
        using PhysicalRegister = RenameBundle::PhysicalRegister;

        bool valid{false};
        std::uint32_t pc{};
        std::uint32_t instruction{};

        Opcode opcode{Opcode::NOP};

        PhysicalRegister physical_rs1{};
        PhysicalRegister physical_rs2{};
        PhysicalRegister physical_rd{};

        std::uint32_t rs1_value{};
        std::uint32_t rs2_value{};
        std::int32_t immediate{};

        ALUOperation alu_operation{ALUOperation::NONE};
        OperandSource operand_a{OperandSource::NONE};
        OperandSource operand_b{OperandSource::NONE};

        bool memory_read{false};
        bool memory_write{false};

        ControlFlow control_flow{ControlFlow::NONE};
        std::size_t rob_index{};

        bool halt{false};
    };
}