#pragma once
#include <cstdint>

#if __has_include("isa/Opcode.hpp")
#include "isa/Opcode.hpp"
#elif __has_include("Opcode.hpp")
#include "Opcode.hpp"
#else
#include "../../../include/isa/Opcode.hpp"
#endif

#include "../rename/RenameBundle.hpp"

namespace cpu
{
    struct ROBEntry
    {
        using PhysicalRegister = RenameBundle::PhysicalRegister;

        //entry state
        bool valid{false};
        bool completed{false};

        //ROB index
        std::size_t rob_index{0};

        //instruction information
        std::uint32_t pc{0};
        std::uint32_t instruction{0};
        Opcode opcode{Opcode::NOP};

        //destination
        bool register_write{false};
        PhysicalRegister physical_rd{0};
        PhysicalRegister old_physical_rd{0};

        //result
        std::uint32_t result{0};
        bool memory_read{false};
        bool memory_write{false};
        std::uint32_t memory_address{0};
        std::uint32_t store_data{0};

        //control flow
        bool branch{false};
        bool branch_taken{false};
        std::uint32_t branch_target{0};
        bool predicted_taken{false};
        std::uint32_t predicted_target{0};
        bool has_prediction{false};
        bool jump{false};
        std::uint32_t jump_target{0};

        //halt
        bool halt{false};

        //branch speculation state
        bool has_checkpoint{false};
        RegisterAliasTable::Checkpoint checkpoint{};
    };
}