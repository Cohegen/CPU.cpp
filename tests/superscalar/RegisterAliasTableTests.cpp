#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../include/isa/Registers.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>

void test_initial_rat()
{
    std::cout << "test_initial_rat ... ";
    cpu::RegisterAliasTable rat;
    // After construction, architectural register i maps to physical register i
    for (std::size_t i = 0; i < cpu::RegisterAliasTable::ArchitecturalRegisterCount; ++i)
    {
        auto arch = static_cast<cpu::Register>(i);
        auto phys = rat.lookup(arch);
        assert(phys == static_cast<cpu::RegisterAliasTable::PhysicalRegister>(i));
    }
    std::cout << "PASSED\n";
}

void test_checkpoint_rename_restore()
{
    std::cout << "test_checkpoint_rename_restore ... ";
    cpu::RegisterAliasTable rat;

    // 1. Save the initial mapping
    rat.checkpoint();
    assert(rat.checkpoint_count() == 1);

    // 2. Rename several registers
    rat.set(cpu::Register::R1, 10);
    rat.set(cpu::Register::R2, 11);
    rat.set(cpu::Register::R3, 12);

    // Verify renames took effect
    assert(rat.lookup(cpu::Register::R1) == 10);
    assert(rat.lookup(cpu::Register::R2) == 11);
    assert(rat.lookup(cpu::Register::R3) == 12);

    // Registers that were NOT renamed remain at their original mapping
    assert(rat.lookup(cpu::Register::R0) == 0);
    assert(rat.lookup(cpu::Register::R4) == 4);

    // 3. Restore from checkpoint
    rat.restore();
    assert(rat.checkpoint_count() == 0);

    // 4. Original mappings recovered
    for (std::size_t i = 0; i < cpu::RegisterAliasTable::ArchitecturalRegisterCount; ++i)
    {
        auto arch = static_cast<cpu::Register>(i);
        auto phys = rat.lookup(arch);
        assert(phys == static_cast<cpu::RegisterAliasTable::PhysicalRegister>(i));
    }
    std::cout << "PASSED\n";
}

void test_nested_checkpoints()
{
    std::cout << "test_nested_checkpoints ... ";
    cpu::RegisterAliasTable rat;

    // First checkpoint (initial state)
    rat.checkpoint();

    // Rename R1
    rat.set(cpu::Register::R1, 20);
    assert(rat.lookup(cpu::Register::R1) == 20);

    // Second checkpoint (with R1 renamed)
    rat.checkpoint();
    assert(rat.checkpoint_count() == 2);

    // Rename R2
    rat.set(cpu::Register::R2, 30);
    assert(rat.lookup(cpu::Register::R2) == 30);

    // Restore second checkpoint -> R2 goes back, R1 stays renamed
    rat.restore();
    assert(rat.lookup(cpu::Register::R1) == 20);
    assert(rat.lookup(cpu::Register::R2) == 2);   // original
    assert(rat.checkpoint_count() == 1);

    // Restore first checkpoint -> everything back to initial
    rat.restore();
    assert(rat.lookup(cpu::Register::R1) == 1);    // original
    assert(rat.lookup(cpu::Register::R2) == 2);    // original
    assert(rat.checkpoint_count() == 0);

    std::cout << "PASSED\n";
}

void test_restore_without_checkpoint()
{
    std::cout << "test_restore_without_checkpoint ... ";
    cpu::RegisterAliasTable rat;

    // Rename something
    rat.set(cpu::Register::R1, 15);
    assert(rat.lookup(cpu::Register::R1) == 15);

    // Restoring with no checkpoint should be a no-op
    rat.restore();
    assert(rat.lookup(cpu::Register::R1) == 15);

    std::cout << "PASSED\n";
}

int main()
{
    test_initial_rat();
    test_checkpoint_rename_restore();
    test_nested_checkpoints();
    test_restore_without_checkpoint();

    std::cout << "\nAll RegisterAliasTable tests PASSED.\n";
    return 0;
}
