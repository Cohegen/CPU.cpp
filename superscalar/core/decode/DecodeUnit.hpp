#pragma once

#include <cstdint>

#if __has_include("isa/Instruction.hpp")
#include "isa/Instruction.hpp"
#include "isa/Opcode.hpp"
#include "isa/InstructionFormat.hpp"
#include "isa/Registers.hpp"
#elif __has_include("Instruction.hpp")
#include "Instruction.hpp"
#include "Opcode.hpp"
#include "InstructionFormat.hpp"
#include "Registers.hpp"
#else
#include "../../../include/isa/Instruction.hpp"
#include "../../../include/isa/Opcode.hpp"
#include "../../../include/isa/InstructionFormat.hpp"
#include "../../../include/isa/Registers.hpp"
#endif

#include "DecodeBundle.hpp"
#include "DecodeTypes.hpp"

namespace cpu
{

class DecodeUnit
{
public:

    DecodeUnit() = default;


    // ------------------------------------------------------------
    // Decode one instruction
    // ------------------------------------------------------------

    [[nodiscard]]
    DecodeBundle decode(
        std::uint32_t pc,
        std::uint32_t raw_instruction,
        bool valid
    ) const noexcept
    {
        DecodeBundle bundle{};

        bundle.valid = valid;
        bundle.pc = pc;
        bundle.instruction = raw_instruction;

        // A bubble contains no instruction.
        if (!valid)
            return bundle;

        Instruction instruction{raw_instruction};

        bundle.opcode = instruction.opcode();

        determine_format(
            bundle.opcode,
            bundle.format
        );

        extract_operands(
            instruction,
            bundle
        );

        decode_control(
            bundle.opcode,
            bundle
        );

        return bundle;
    }


    // ------------------------------------------------------------
    // Decode two instructions simultaneously
    // ------------------------------------------------------------

    void decode(
        std::uint32_t pc0,
        std::uint32_t instruction0,
        bool valid0,

        std::uint32_t pc1,
        std::uint32_t instruction1,
        bool valid1,

        DecodeBundle& output0,
        DecodeBundle& output1
    ) const noexcept
    {
        output0 = decode(
            pc0,
            instruction0,
            valid0
        );

        output1 = decode(
            pc1,
            instruction1,
            valid1
        );
    }


private:

    // ------------------------------------------------------------
    // Determine instruction format
    // ------------------------------------------------------------

    static void determine_format(
        Opcode opcode,
        InstructionFormat& format
    ) noexcept
    {
        switch (opcode)
        {
            case Opcode::ADD:
            case Opcode::SUB:
            case Opcode::AND:
            case Opcode::OR:
            case Opcode::XOR:
            case Opcode::NOT:

                format = InstructionFormat::R_TYPE;
                break;


            case Opcode::LI:
            case Opcode::ADDI:
            case Opcode::LW:
            case Opcode::NOP:
            case Opcode::HALT:

                format = InstructionFormat::I_TYPE;
                break;


            case Opcode::SW:

                format = InstructionFormat::S_TYPE;
                break;


            case Opcode::BEQ:
            case Opcode::BNE:

                format = InstructionFormat::B_TYPE;
                break;


            case Opcode::J:

                format = InstructionFormat::J_TYPE;
                break;
        }
    }


    // ------------------------------------------------------------
    // Extract architectural operands
    // ------------------------------------------------------------

    static void extract_operands(
        const Instruction& instruction,
        DecodeBundle& bundle
    ) noexcept
    {
        switch (bundle.format)
        {
            case InstructionFormat::R_TYPE:
            {
                bundle.rd = instruction.rd();
                bundle.rs1 = instruction.rs1();
                bundle.rs2 = instruction.rs2();

                bundle.immediate = 0;

                break;
            }


            case InstructionFormat::I_TYPE:
            {
                bundle.rd = instruction.rd();
                bundle.rs1 = instruction.rs1();
                bundle.rs2 = Register::R0;

                bundle.immediate = instruction.immediate();

                break;
            }


            case InstructionFormat::S_TYPE:
            {
                /*
                 * S-type:
                 *
                 * [opcode][rs2][rs1][immediate]
                 *          ↑     ↑
                 *          rd()  rs1()
                 */

                bundle.rd = Register::R0;

                bundle.rs2 = instruction.rd();
                bundle.rs1 = instruction.rs1();

                bundle.immediate = instruction.immediate();

                break;
            }


            case InstructionFormat::B_TYPE:
            {
                /*
                 * B-type:
                 *
                 * [opcode][rs1][rs2][immediate]
                 *          ↑     ↑
                 *          rd()  rs1()
                 */

                bundle.rd = Register::R0;

                bundle.rs1 = instruction.rd();
                bundle.rs2 = instruction.rs1();

                bundle.immediate = instruction.immediate();

                break;
            }


            case InstructionFormat::J_TYPE:
            {
                bundle.rd = Register::R0;
                bundle.rs1 = Register::R0;
                bundle.rs2 = Register::R0;

                bundle.immediate =
                    extract_jump_immediate(
                        instruction.raw()
                    );

                break;
            }
        }
    }


    // ------------------------------------------------------------
    // Decode execution/control information
    // ------------------------------------------------------------

    static void decode_control(
        Opcode opcode,
        DecodeBundle& bundle
    ) noexcept
    {
        switch (opcode)
        {
            // ----------------------------------------------------
            // NOP
            // ----------------------------------------------------

            case Opcode::NOP:
            {
                break;
            }


            // ----------------------------------------------------
            // R-type ALU operations
            // ----------------------------------------------------

            case Opcode::ADD:
            {
                bundle.alu_operation = ALUOperation::ADD;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::REGISTER;

                bundle.register_write = true;

                break;
            }


            case Opcode::SUB:
            {
                bundle.alu_operation = ALUOperation::SUB;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::REGISTER;

                bundle.register_write = true;

                break;
            }


            case Opcode::AND:
            {
                bundle.alu_operation = ALUOperation::AND;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::REGISTER;

                bundle.register_write = true;

                break;
            }


            case Opcode::OR:
            {
                bundle.alu_operation = ALUOperation::OR;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::REGISTER;

                bundle.register_write = true;

                break;
            }


            case Opcode::XOR:
            {
                bundle.alu_operation = ALUOperation::XOR;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::REGISTER;

                bundle.register_write = true;

                break;
            }


            case Opcode::NOT:
            {
                bundle.alu_operation = ALUOperation::NOT;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::NONE;

                bundle.register_write = true;

                break;
            }


            // ----------------------------------------------------
            // Immediate operations
            // ----------------------------------------------------

            case Opcode::LI:
            {
                /*
                 * LI:
                 *
                 * rd = immediate
                 */

                bundle.alu_operation = ALUOperation::ADD;

                bundle.operand_a = OperandSource::NONE;
                bundle.operand_b = OperandSource::IMMEDIATE;

                bundle.register_write = true;

                break;
            }


            case Opcode::ADDI:
            {
                /*
                 * rd = rs1 + immediate
                 */

                bundle.alu_operation = ALUOperation::ADD;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::IMMEDIATE;

                bundle.register_write = true;

                break;
            }


            // ----------------------------------------------------
            // Load
            // ----------------------------------------------------

            case Opcode::LW:
            {
                /*
                 * Effective address:
                 *
                 * rs1 + immediate
                 */

                bundle.alu_operation = ALUOperation::ADD;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::IMMEDIATE;

                bundle.register_write = true;
                bundle.memory_read = true;

                break;
            }


            // ----------------------------------------------------
            // Store
            // ----------------------------------------------------

            case Opcode::SW:
            {
                /*
                 * Effective address:
                 *
                 * rs1 + immediate
                 *
                 * Data:
                 *
                 * rs2
                 */

                bundle.alu_operation = ALUOperation::ADD;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::IMMEDIATE;

                bundle.register_write = false;
                bundle.memory_write = true;

                break;
            }


            // ----------------------------------------------------
            // Branches
            // ----------------------------------------------------

            case Opcode::BEQ:
            {
                bundle.alu_operation = ALUOperation::SUB;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::REGISTER;

                bundle.register_write = false;

                bundle.control_flow = ControlFlow::BRANCH;

                break;
            }


            case Opcode::BNE:
            {
                bundle.alu_operation = ALUOperation::SUB;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::REGISTER;

                bundle.register_write = false;

                bundle.control_flow = ControlFlow::BRANCH;

                break;
            }


            // ----------------------------------------------------
            // Jump
            // ----------------------------------------------------

            case Opcode::J:
            {
                bundle.register_write = false;

                bundle.control_flow = ControlFlow::JUMP;

                break;
            }


            // ----------------------------------------------------
            // Halt
            // ----------------------------------------------------

            case Opcode::HALT:
            {
                bundle.halt = true;

                break;
            }
        }
    }


    
    // Extracting J-type 26-bit signed immediate

    [[nodiscard]]
    static std::int32_t extract_jump_immediate(
        std::uint32_t raw
    ) noexcept
    {
        constexpr std::uint32_t ImmediateMask = 0x03FFFFFFU;
        constexpr std::uint32_t SignBit = 0x02000000U;

        std::uint32_t immediate =
            raw & ImmediateMask;

        if ((immediate & SignBit) != 0)
        {
            immediate |= ~ImmediateMask;
        }

        return static_cast<std::int32_t>(immediate);
    }
};

} 