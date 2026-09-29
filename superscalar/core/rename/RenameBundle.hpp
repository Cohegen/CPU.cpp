#pragma once

#include <cstdint>

#if __has_include("isa/Opcode.hpp")
#include "isa/Opcode.hpp"
#include "isa/Registers.hpp"
#include "isa/InstructionFormat.hpp"
#elif __has_include("Opcode.hpp")
#include "Opcode.hpp"
#include "Registers.hpp"
#include "InstructionFormat.hpp"
#else
#include "../../../include/isa/Opcode.hpp"
#include "../../../include/isa/Registers.hpp"
#include "../../../include/isa/InstructionFormat.hpp"
#endif

#include "../decode/DecodeTypes.hpp"
#include "PhysicalRegisterFreeList.hpp"

namespace cpu
{
    struct RenameBundle
    {
        // Instruction state & identity
        bool valid{false};
        std::uint32_t pc{0};
        std::uint32_t instruction{0};
        Opcode opcode{Opcode::NOP};
        InstructionFormat format{InstructionFormat::I_TYPE};

        // Architectural register fields
        Register rd{Register::R0};
        Register rs1{Register::R0};
        Register rs2{Register::R0};

        // Physical registers
        using PhysicalRegister = std::uint8_t;
        PhysicalRegister physical_rd{PhysicalRegisterFreeList::INVALID_REGISTER};
        PhysicalRegister physical_rs1{0};
        PhysicalRegister physical_rs2{0};

        // Immediate
        std::int32_t immediate{0};

        // Execution information
        ALUOperation alu_operation{ALUOperation::NONE};
        OperandSource operand_a{OperandSource::NONE};
        OperandSource operand_b{OperandSource::NONE};

        // Control information
        bool register_write{false};
        bool memory_read{false};
        bool memory_write{false};
        ControlFlow control_flow{ControlFlow::NONE};
        bool halt{false};
    };
}