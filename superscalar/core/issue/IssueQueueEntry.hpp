#pragma once
#include <cstdint>
#include <cstddef>

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
    struct IssueQueueEntry
    {
        using PhysicalRegister = RenameBundle::PhysicalRegister;

        //entry state
        bool valid{false};
        bool issued{false};

        //instruction information
        std::uint32_t pc{0};
        std::uint32_t instruction{0};
        Opcode opcode{Opcode::NOP};

        //physical operands
        PhysicalRegister physical_rs1{0};
        PhysicalRegister physical_rs2{0};
        PhysicalRegister physical_rd{0};

        //operand readiness
        bool rs1_ready{false};
        bool rs2_ready{false};

        //operand values
        std::uint32_t rs1_value{0};
        std::uint32_t rs2_value{0};

        //immediate
        std::int32_t immediate{0};

        //execution control
        ALUOperation alu_operation{ALUOperation::NONE};
        ALUOperation alu_operand{ALUOperation::NONE};
        OperandSource operand_a{OperandSource::NONE};
        OperandSource operand_b{OperandSource::NONE};

        //register write & memory
        bool register_write{false};
        bool memory_read{false};
        bool memory_write{false};

        //control flow
        ControlFlow control_flow{ControlFlow::NONE};

        //ROB identity
        std::size_t rob_index{0};

        //branch prediction
        bool predicted_taken{false};
        std::uint32_t predicted_target{0};

        //halt
        bool halt{false};

    };
}