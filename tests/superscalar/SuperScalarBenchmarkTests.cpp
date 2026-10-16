#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstdint>
#include <vector>

#include "../../components/SuperScalarCPU.hpp"
#include "../../superscalar/core/memory/data/DataCache.hpp"

using namespace cpu;

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
// Benchmark 1: High-ILP Dual-Issue Arithmetic Benchmark
// Evaluates superscalar instruction throughput (IPC) with pairwise independent instructions.
// ============================================================================
void benchmark_high_ilp_arithmetic()
{
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "[Benchmark 1] High-ILP Dual-Issue Arithmetic Benchmark" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    SuperScalarCPU cpu;

    // Independent operations can issue concurrently across dual integer ALUs
    std::vector<std::size_t> program = {
        encode_i(Opcode::LI,   Register::R1, Register::R0, 10),
        encode_i(Opcode::LI,   Register::R2, Register::R0, 20),
        encode_i(Opcode::ADDI, Register::R3, Register::R1, 5),     // R3 = 15
        encode_i(Opcode::ADDI, Register::R4, Register::R2, 15),    // R4 = 35
        encode_i(Opcode::ADDI, Register::R5, Register::R1, 100),   // R5 = 110
        encode_i(Opcode::ADDI, Register::R6, Register::R2, 200),   // R6 = 220
        encode_r(Opcode::ADD,  Register::R7, Register::R1, Register::R2), // R7 = 30
        encode_r(Opcode::ADD,  Register::R8, Register::R3, Register::R4), // R8 = 50
        encode_halt()
    };

    cpu.load_program(program);
    cpu.run(100);

    assert(cpu.halted());
    assert(cpu.read_register(Register::R3) == 15);
    assert(cpu.read_register(Register::R4) == 35);
    assert(cpu.read_register(Register::R5) == 110);
    assert(cpu.read_register(Register::R6) == 220);
    assert(cpu.read_register(Register::R7) == 30);
    assert(cpu.read_register(Register::R8) == 50);

    const std::size_t retired = cpu.committed_instructions();
    const std::size_t cycles = cpu.cycles();
    const double ipc = cpu.ipc();

    std::cout << "  Retired Instructions: " << retired << std::endl;
    std::cout << "  Elapsed Cycles:       " << cycles << std::endl;
    std::cout << "  Measured IPC:         " << std::fixed << std::setprecision(2) << ipc << std::endl;

    // Verify superscalar advantage: IPC must exceed scalar theoretical maximum (IPC > 1.0)
    assert(ipc > 1.0);
    std::cout << "  [PASS] Superscalar advantage verified: IPC = " << ipc << " > 1.0!" << std::endl;
}

// ============================================================================
// Benchmark 2: Iterative Fibonacci Sequence Benchmark
// Computes F(10) = 55 iteratively, testing loop performance and branch prediction.
// ============================================================================
void benchmark_fibonacci_sequence()
{
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "[Benchmark 2] Iterative Fibonacci Sequence Benchmark" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    SuperScalarCPU cpu;

    // Compute F(10):
    // R1 = F(n-1) = 0
    // R2 = F(n)   = 1
    // R3 = Loop count = 9
    // loop (PC 0x0C):
    //   ADD  R4, R1, R2     (R4 = next fib)
    //   ADDI R1, R2, 0      (R1 = old R2)
    //   ADDI R2, R4, 0      (R2 = new fib)
    //   ADDI R3, R3, -1     (R3 -= 1)
    //   BNE  R3, R0, -16    (branch to loop, -16 bytes)
    // HALT
    std::vector<std::size_t> program = {
        /* 0x00 */ encode_i(Opcode::LI,   Register::R1, Register::R0, 0),
        /* 0x04 */ encode_i(Opcode::LI,   Register::R2, Register::R0, 1),
        /* 0x08 */ encode_i(Opcode::LI,   Register::R3, Register::R0, 9),
        /* 0x0C */ encode_r(Opcode::ADD,  Register::R4, Register::R1, Register::R2),
        /* 0x10 */ encode_i(Opcode::ADDI, Register::R1, Register::R2, 0),
        /* 0x14 */ encode_i(Opcode::ADDI, Register::R2, Register::R4, 0),
        /* 0x18 */ encode_i(Opcode::ADDI, Register::R3, Register::R3, -1),
        /* 0x1C */ encode_b(Opcode::BNE,  Register::R3, Register::R0, -16),
        /* 0x20 */ encode_halt()
    };

    cpu.load_program(program);
    cpu.run(200);

    assert(cpu.halted());
    // F(10) = 55
    assert(cpu.read_register(Register::R2) == 55);
    assert(cpu.read_register(Register::R3) == 0);

    const std::size_t branches = cpu.branches_executed();
    const std::size_t mispredictions = cpu.branch_mispredictions();
    const double accuracy = cpu.branch_accuracy();

    std::cout << "  Computed F(10):       " << cpu.read_register(Register::R2) << " (expected 55)" << std::endl;
    std::cout << "  Retired Instructions: " << cpu.committed_instructions() << std::endl;
    std::cout << "  Total Cycles:         " << cpu.cycles() << std::endl;
    std::cout << "  Branches Executed:    " << branches << std::endl;
    std::cout << "  Mispredictions:       " << mispredictions << std::endl;
    std::cout << "  Branch Accuracy:      " << std::fixed << std::setprecision(1) << accuracy << "%" << std::endl;

    assert(branches >= 9);
    assert(accuracy >= 75.0); // 2-bit branch predictor trains quickly to taken loop branch
    std::cout << "  [PASS] Fibonacci computation and branch predictor verified!" << std::endl;
}

// ============================================================================
// Benchmark 3: Memory Disambiguation and D-Cache Benchmark
// Tests out-of-order store-to-load forwarding and data cache line caching.
// ============================================================================
void benchmark_memory_forwarding_and_dcache()
{
    std::cout << "\n------------------------------------------------------------" << std::endl;
    std::cout << "[Benchmark 3] Memory Forwarding & Data Cache Benchmark" << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;

    SuperScalarCPU cpu;

    std::vector<std::size_t> program = {
        encode_i(Opcode::LI,   Register::R1, Register::R0, 42),
        encode_i(Opcode::LI,   Register::R2, Register::R0, 84),
        encode_s(Opcode::SW,   Register::R1, Register::R0, 0),     // mem[0] = 42
        encode_s(Opcode::SW,   Register::R2, Register::R0, 4),     // mem[4] = 84
        encode_i(Opcode::LW,   Register::R3, Register::R0, 0),     // R3 = 42 (forwarded / loaded)
        encode_i(Opcode::LW,   Register::R4, Register::R0, 4),     // R4 = 84 (forwarded / loaded)
        encode_r(Opcode::ADD,  Register::R5, Register::R3, Register::R4), // R5 = 126
        encode_s(Opcode::SW,   Register::R5, Register::R0, 8),     // mem[8] = 126
        encode_i(Opcode::LW,   Register::R6, Register::R0, 8),     // R6 = 126
        encode_halt()
    };

    cpu.load_program(program);
    cpu.run(100);

    assert(cpu.halted());
    assert(cpu.read_register(Register::R3) == 42);
    assert(cpu.read_register(Register::R4) == 84);
    assert(cpu.read_register(Register::R5) == 126);
    assert(cpu.read_register(Register::R6) == 126);
    assert(cpu.read_memory(0) == 42);
    assert(cpu.read_memory(1) == 84);
    assert(cpu.read_memory(2) == 126);

    std::cout << "  ARF State: R3=" << cpu.read_register(Register::R3)
              << ", R4=" << cpu.read_register(Register::R4)
              << ", R5=" << cpu.read_register(Register::R5)
              << ", R6=" << cpu.read_register(Register::R6) << std::endl;
    std::cout << "  Committed Memory: word[0]=" << cpu.read_memory(0)
              << ", word[1]=" << cpu.read_memory(1)
              << ", word[2]=" << cpu.read_memory(2) << std::endl;
    std::cout << "  [PASS] Memory forwarding and commit completed successfully." << std::endl;
}

int main()
{
    std::cout << "============================================================" << std::endl;
    std::cout << "   SUPERSCALAR CPU MICROARCHITECTURAL BENCHMARK SUITE       " << std::endl;
    std::cout << "============================================================" << std::endl;

    benchmark_high_ilp_arithmetic();
    benchmark_fibonacci_sequence();
    benchmark_memory_forwarding_and_dcache();

    std::cout << "\n============================================================" << std::endl;
    std::cout << "   ALL SUPERSCALAR BENCHMARKS PASSED SUCCESSFULLY!          " << std::endl;
    std::cout << "============================================================" << std::endl;

    return 0;
}
