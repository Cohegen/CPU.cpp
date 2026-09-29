#pragma once

#include <cstddef>
#include <cstdint>

#include "../decode/DecodeBundle.hpp"
#include "PhysicalRegisterFreeList.hpp"
#include "RegisterAliasTable.hpp"
#include "RenameBundle.hpp"

namespace cpu
{
    class RenameUnit
    {
    public:
        using PhysicalRegister = RenameBundle::PhysicalRegister;

        RenameUnit(RegisterAliasTable& rat, PhysicalRegisterFreeList& free_list) noexcept
            : rat_(rat),
              free_list_(free_list)
        {
        }

        // --------------------------------------------------------------------
        // Check if an instruction or bundle can be renamed without stalling
        // --------------------------------------------------------------------
        [[nodiscard]]
        bool can_rename(const DecodeBundle& input) const noexcept
        {
            if (!input.valid || !input.register_write)
            {
                return true;
            }
            return !free_list_.empty();
        }

        [[nodiscard]]
        bool can_rename(const DecodeBundle& input0, const DecodeBundle& input1) const noexcept
        {
            std::size_t needed = 0;
            if (input0.valid && input0.register_write)
            {
                ++needed;
            }
            if (input1.valid && input1.register_write)
            {
                ++needed;
            }
            return free_list_.size() >= needed;
        }

        // --------------------------------------------------------------------
        // Single instruction rename
        // --------------------------------------------------------------------
        [[nodiscard]]
        RenameBundle rename(const DecodeBundle& input) noexcept
        {
            RenameBundle output{};

            // Bubble / invalid instruction produces a bubble
            if (!input.valid)
            {
                output.valid = false;
                return output;
            }

            // Preserving instruction information
            output.valid = true;
            output.pc = input.pc;
            output.instruction = input.instruction;
            output.opcode = input.opcode;
            output.format = input.format;
            output.immediate = input.immediate;

            // Architectural registers
            output.rd = input.rd;
            output.rs1 = input.rs1;
            output.rs2 = input.rs2;

            // Preserving execution control
            output.alu_operation = input.alu_operation;
            output.operand_a = input.operand_a;
            output.operand_b = input.operand_b;
            output.register_write = input.register_write;
            output.memory_read = input.memory_read;
            output.memory_write = input.memory_write;
            output.control_flow = input.control_flow;
            output.halt = input.halt;

            // Source register renaming: translated through RAT
            output.physical_rs1 = rat_.lookup(input.rs1);
            output.physical_rs2 = rat_.lookup(input.rs2);

            // Destination register renaming
            if (input.register_write)
            {
                const PhysicalRegister new_physical_rd = free_list_.allocate();
                if (new_physical_rd == PhysicalRegisterFreeList::INVALID_REGISTER)
                {
                    // Physical register exhaustion stalls / invalidates rename
                    output.valid = false;
                    output.physical_rd = PhysicalRegisterFreeList::INVALID_REGISTER;
                    stalled_ = true;
                    return output;
                }

                output.physical_rd = new_physical_rd;

                // Update RAT with new mapping
                rat_.set(input.rd, new_physical_rd);
            }
            else
            {
                // Instructions with register_write = false don't allocate a physical register
                output.physical_rd = PhysicalRegisterFreeList::INVALID_REGISTER;
            }

            return output;
        }

        // --------------------------------------------------------------------
        // 2-wide superscalar rename in one cycle
        // --------------------------------------------------------------------
        void rename(
            const DecodeBundle& input0,
            const DecodeBundle& input1,
            RenameBundle& output0,
            RenameBundle& output1
        ) noexcept
        {
            stalled_ = false;

            // Rename lane 0 first
            output0 = rename(input0);

            // If lane 0 was a valid instruction that failed due to register exhaustion,
            // the pipeline stalls and younger instruction (lane 1) cannot proceed.
            if (input0.valid && !output0.valid)
            {
                output1 = RenameBundle{};
                output1.valid = false;
                stalled_ = true;
                return;
            }

            // Rename lane 1: lane 1 sees lane 0's RAT update
            output1 = rename(input1);

            // If lane 1 was valid and failed due to register exhaustion, record stall
            if (input1.valid && !output1.valid)
            {
                stalled_ = true;
            }
        }

        // --------------------------------------------------------------------
        // State inspectors and helpers
        // --------------------------------------------------------------------
        [[nodiscard]]
        bool stalled() const noexcept
        {
            return stalled_;
        }

        [[nodiscard]]
        bool is_stalled() const noexcept
        {
            return stalled_;
        }

        void clear_stall() noexcept
        {
            stalled_ = false;
        }

        [[nodiscard]]
        const RegisterAliasTable& rat() const noexcept
        {
            return rat_;
        }

        [[nodiscard]]
        RegisterAliasTable& rat() noexcept
        {
            return rat_;
        }

        [[nodiscard]]
        const PhysicalRegisterFreeList& free_list() const noexcept
        {
            return free_list_;
        }

        [[nodiscard]]
        PhysicalRegisterFreeList& free_list() noexcept
        {
            return free_list_;
        }

    private:
        RegisterAliasTable& rat_;
        PhysicalRegisterFreeList& free_list_;
        bool stalled_{false};
    };
}