#include <iostream>
#include <cassert>
#include <cstdint>
#include <vector>

#include "../../superscalar/datapath/SuperScalarCPU.hpp"
#include "../../include/isa/Opcode.hpp"
#include "../../include/isa/Registers.hpp"

using namespace cpu;

// Helper instruction encoders matching our ISA specification
// Opcode (bits 31..26), rd (25..22), rs1 (21..18), rs2 (17..14), imm (17..0)
static constexpr std::uint32_t encode_r(Opcode op, Register rd, Register rs1, Register rs2)
{
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (static_cast<std::uint32_t>(rs2) << 14);
}

static constexpr std::uint32_t encode_i(Opcode op, Register rd, Register rs1, std::int32_t imm)
{
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (static_cast<std::uint32_t>(imm) & 0x3FFFF);
}

static constexpr std::uint32_t encode_s(Opcode op, Register rs2, Register rs1, std::int32_t imm)
{
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rs2) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (static_cast<std::uint32_t>(imm) & 0x3FFFF);
}

static constexpr std::uint32_t encode_b(Opcode op, Register rs1, Register rs2, std::int32_t imm)
{
    return (static_cast<std::uint32_t>(op) << 26) |
           (static_cast<std::uint32_t>(rs1) << 22) |
           (static_cast<std::uint32_t>(rs2) << 18) |
           (static_cast<std::uint32_t>(imm) & 0x3FFFF);
}

static constexpr std::uint32_t encode_halt()
{
    return (static_cast<std::uint32_t>(Opcode::HALT) << 26);
}

// ============================================================================
// Test 1: Basic Arithmetic and Register Forwarding Program
// LI R1, 10
// LI R2, 20
// ADD R3, R1, R2   -> R3 = 30
// ADDI R4, R3, 5   -> R4 = 35
// HALT
// ============================================================================
void test_arithmetic_sequence()
{
    std::cout << "[Test 1] Testing End-to-End Arithmetic Sequence..." << std::endl;

    SuperScalarCPU cpu;

    std::vector<std::size_t> program = {
        encode_i(Opcode::LI, Register::R1, Register::R0, 10),
        encode_i(Opcode::LI, Register::R2, Register::R0, 20),
        encode_r(Opcode::ADD, Register::R3, Register::R1, Register::R2),
        encode_i(Opcode::ADDI, Register::R4, Register::R3, 5),
        encode_halt()
    };

    cpu.load_program(program);

    std::size_t cycles = 0;
    for (std::size_t c = 0; c < 20; ++c)
    {
        auto stats = cpu.step();
        cycles = stats.cycle;
        std::cout << "Cycle " << stats.cycle
                  << ": fetched=" << stats.fetched
                  << " decoded=" << stats.decoded
                  << " dispatched=" << stats.dispatched
                  << " issued_alu=" << stats.issued_alu
                  << " retired=" << stats.retired
                  << " halted=" << stats.halted
                  << std::endl;
        if (stats.halted) break;
    }

    assert(cpu.halted());
    assert(cpu.committed_instruction_count() == 5);

    // Verify architectural register file
    assert(cpu.read_arf(Register::R1) == 10);
    assert(cpu.read_arf(Register::R2) == 20);
    assert(cpu.read_arf(Register::R3) == 30);
    assert(cpu.read_arf(Register::R4) == 35);

    std::cout << "  [PASS] Arithmetic sequence finished in " << cycles
              << " cycles with correct ARF state." << std::endl;
}

// ============================================================================
// Test 2: Memory Read/Write (Store -> Load) Program
// LI R1, 42
// SW R1, 0(R0)      -> mem[0] = 42
// LW R2, 0(R0)      -> R2 = 42
// ADDI R3, R2, 8    -> R3 = 50
// HALT
// ============================================================================
void test_memory_sequence()
{
    std::cout << "[Test 2] Testing End-to-End Memory Sequence (SW -> LW)..." << std::endl;

    SuperScalarCPU cpu;

    std::vector<std::size_t> program = {
        encode_i(Opcode::LI, Register::R1, Register::R0, 42),
        encode_s(Opcode::SW, Register::R1, Register::R0, 0),
        encode_i(Opcode::LW, Register::R2, Register::R0, 0),
        encode_i(Opcode::ADDI, Register::R3, Register::R2, 8),
        encode_halt()
    };

    cpu.load_program(program);

    std::size_t cycles = 0;
    for (std::size_t c = 0; c < 20; ++c)
    {
        auto stats = cpu.step();
        cycles = stats.cycle;
        std::cout << "Cycle " << stats.cycle
                  << ": dispatched=" << stats.dispatched
                  << " issued_alu=" << stats.issued_alu
                  << " issued_mem=" << stats.issued_mem
                  << " retired=" << stats.retired
                  << " halted=" << stats.halted
                  << std::endl;
        if (stats.halted) break;
    }

    assert(cpu.halted());
    assert(cpu.committed_instruction_count() == 5);

    std::cout << "  ARF R1=" << cpu.read_arf(Register::R1)
              << " R2=" << cpu.read_arf(Register::R2)
              << " R3=" << cpu.read_arf(Register::R3)
              << " mem[0]=" << cpu.get_data_word(0) << std::endl;

    assert(cpu.read_arf(Register::R1) == 42);
    assert(cpu.read_arf(Register::R2) == 42);
    assert(cpu.read_arf(Register::R3) == 50);

    // Data memory was committed to word 0
    assert(cpu.get_data_word(0) == 42);

    std::cout << "  [PASS] Memory sequence finished in " << cycles
              << " cycles with correct memory and ARF state." << std::endl;
}

// ============================================================================
// Test 3: Branch Control Flow & Loop
// Simple decrement loop:
// LI R1, 3           ; loop counter = 3
// LI R2, 0           ; accumulator = 0
// loop:
// ADDI R2, R2, 10    ; acc += 10
// ADDI R1, R1, -1    ; counter -= 1
// BNE R1, R0, -8     ; if counter != 0 goto loop (PC relative -8 bytes)
// HALT
// ============================================================================
void test_branch_loop()
{
    std::cout << "[Test 3] Testing End-to-End Branch Loop Execution..." << std::endl;

    SuperScalarCPU cpu;

    std::vector<std::size_t> program = {
        /* PC 0x00 */ encode_i(Opcode::LI, Register::R1, Register::R0, 3),
        /* PC 0x04 */ encode_i(Opcode::LI, Register::R2, Register::R0, 0),
        /* PC 0x08 */ encode_i(Opcode::ADDI, Register::R2, Register::R2, 10),
        /* PC 0x0C */ encode_i(Opcode::ADDI, Register::R1, Register::R1, -1),
        /* PC 0x10 */ encode_b(Opcode::BNE, Register::R1, Register::R0, -8),
        /* PC 0x14 */ encode_halt()
    };

    cpu.load_program(program);

    std::size_t cycles = 0;
    for (std::size_t c = 0; c < 50; ++c)
    {
        auto stats = cpu.step();
        cycles = stats.cycle;
        std::cout << "Cycle " << stats.cycle
                  << ": dispatched=" << stats.dispatched
                  << " issued_alu=" << stats.issued_alu
                  << " issued_branch=" << stats.issued_branch
                  << " mispred=" << stats.branch_mispredicted
                  << " redirect=" << stats.redirect_pc
                  << " retired=" << stats.retired
                  << " halted=" << stats.halted
                  << std::endl;
        if (stats.halted) break;
    }

    assert(cpu.halted());

    std::cout << "  Branch loop halted: ARF R1=" << cpu.read_arf(Register::R1)
              << " R2=" << cpu.read_arf(Register::R2) << std::endl;

    // After 3 iterations: R1 = 0, R2 = 30
    assert(cpu.read_arf(Register::R1) == 0);
    assert(cpu.read_arf(Register::R2) == 30);

    std::cout << "  [PASS] Branch loop completed successfully in " << cycles
              << " cycles: R2 = 30, R1 = 0." << std::endl;
}

// ============================================================================
// Test 4: RAW Hazard Resolution Across Superscalar Bundles
// Testing dependencies between adjacent instructions within the same fetch bundle
// LI R1, 100
// ADDI R2, R1, 50     -> R2 depends on R1 from previous instruction
// ADD R3, R1, R2      -> R3 = 100 + 150 = 250
// HALT
// ============================================================================
void test_superscalar_raw_hazards()
{
    std::cout << "[Test 4] Testing Superscalar RAW Hazard Resolution..." << std::endl;

    SuperScalarCPU cpu;

    std::vector<std::size_t> program = {
        encode_i(Opcode::LI, Register::R1, Register::R0, 100),
        encode_i(Opcode::ADDI, Register::R2, Register::R1, 50),
        encode_r(Opcode::ADD, Register::R3, Register::R1, Register::R2),
        encode_halt()
    };

    cpu.load_program(program);

    const std::size_t cycles = cpu.run(50);
    assert(cpu.halted());
    assert(cpu.read_arf(Register::R1) == 100);
    assert(cpu.read_arf(Register::R2) == 150);
    assert(cpu.read_arf(Register::R3) == 250);

    std::cout << "  [PASS] RAW dependencies correctly resolved in " << cycles
              << " cycles." << std::endl;
}

int main()
{
    std::cout << "========================================================" << std::endl;
    std::cout << "Starting Phase 3: Top-Level SuperScalarCPU Integration Tests" << std::endl;
    std::cout << "========================================================" << std::endl;

    test_arithmetic_sequence();
    test_memory_sequence();
    test_branch_loop();
    test_superscalar_raw_hazards();

    std::cout << "========================================================" << std::endl;
    std::cout << "ALL Phase 3 SuperScalarCPU Integration Tests PASSED!" << std::endl;
    std::cout << "========================================================" << std::endl;

    return 0;
}
