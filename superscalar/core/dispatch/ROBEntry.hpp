#pragma once
#include <cstdint>
#include "../include/Opcode.hpp"
#include "../rename/RenameUnit.hpp"

namespace cpu
{
    struct{
        using PhysicalRegister = RenameBundle::PhysicalRegister;

        //entry state
        bool valid{false};
        bool completed{false};

        //instruction information
        std::uint32_t pc{0};
        std::uint32_t instruction{0};
        Opcode opcode{Opcode::NOP};

        //destination
        bool register_write{false};
        PhysicalRegister physical_rd{0};
        PhysicalRegister old_physical_rd{0};

        //result
        bool memory_read{false};
        bool memory_write{false};
        std::uint32_t memory_address{0};
        std::uint_32_t store_data{0};

        //control flow
        bool branch{false};
        bool branch_taken{false};
        std::uint32_t branch_target{0};
        bool jump{false};
        std::uint32_t jump_target{0};

        //halt
        bool halt{false};
    };
}