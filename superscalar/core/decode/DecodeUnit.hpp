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

#if __has_include("isa/InstructionDecoder.hpp")
#include "isa/InstructionDecoder.hpp"
#else
#include "../../../include/isa/InstructionDecoder.hpp"
#endif

namespace cpu
{

class DecodeUnit
{
public:

    DecodeUnit() = default;


   
    // Decode one instruction
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

        // Instruction identity, format, field extraction, and immediate
        // sign-extension are shared with every other CPU organization.
        const DecodedInstruction decoded = InstructionDecoder::decode(instruction);
        bundle.opcode = decoded.opcode;
        bundle.format = decoded.format;
        bundle.rd = decoded.rd;
        bundle.rs1 = decoded.rs1;
        bundle.rs2 = decoded.rs2;
        bundle.immediate = decoded.immediate;

        decode_control(
            bundle.opcode,
            bundle
        );

        return bundle;
    }


 
    // Decodes two instructions simultaneously
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

    // Superscalar-specific control production follows the shared ISA decode.

    static void decode_control(
        Opcode opcode,
        DecodeBundle& bundle
    ) noexcept
    {
        switch (opcode)
        {
          
            // NOP
            case Opcode::NOP:
            {
                break;
            }


            // R-type ALU operations
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


            
            // Immediate operations
            case Opcode::LI:
            {
                /*
                Loading an immediate
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
                  rd = rs1 + immediate
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
                 
                  rs1 + immediate
                 */

                bundle.alu_operation = ALUOperation::ADD;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::IMMEDIATE;

                bundle.register_write = true;
                bundle.memory_read = true;

                break;
            }


   
            // Store
           case Opcode::SW:
            {
                /*
                 * Effective address:
                
                  rs1 + immediate
                 
                  Data:
                 
                  rs2
                 */

                bundle.alu_operation = ALUOperation::ADD;

                bundle.operand_a = OperandSource::REGISTER;
                bundle.operand_b = OperandSource::IMMEDIATE;

                bundle.register_write = false;
                bundle.memory_write = true;

                break;
            }


         
            // Branches
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


           
            // Jump
            case Opcode::J:
            {
                bundle.register_write = false;

                bundle.control_flow = ControlFlow::JUMP;

                break;
            }


          
            // Halt
            case Opcode::HALT:
            {
                bundle.halt = true;

                break;
            }
        }
    }
};

} 
