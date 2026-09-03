#include "../../multi_cycle_cpu/datapath/MultiCycleDatapath.hpp"

#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using Datapath = cpu::MultiCycleDatapath<32, 32, 32, 4, 8>;

std::uint32_t encode_r_type(
    cpu::Opcode opcode,
    cpu::Register rd,
    cpu::Register rs1,
    cpu::Register rs2
)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (static_cast<std::uint32_t>(rs2) << 14);
}

std::uint32_t encode_i_type(
    cpu::Opcode opcode,
    cpu::Register rd,
    cpu::Register rs1,
    std::uint32_t immediate
)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rd) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (immediate & 0x3FFFFU);
}

std::uint32_t encode_s_type(
    cpu::Opcode opcode,
    cpu::Register rs2,
    cpu::Register rs1,
    std::uint32_t immediate
)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(rs2) << 22) |
           (static_cast<std::uint32_t>(rs1) << 18) |
           (immediate & 0x3FFFFU);
}

std::uint32_t encode_j_type(cpu::Opcode opcode, std::int32_t immediate)
{
    return (static_cast<std::uint32_t>(opcode) << 26) |
           (static_cast<std::uint32_t>(immediate) & 0x03FFFFFFU);
}

cpu::MultiCycleControlSignals fetch_cycle()
{
    cpu::MultiCycleControlSignals c;
    c.memRead = true;
    c.irWrite = true;
    c.iorD = false;
    c.aluSrcA = cpu::ALUSrcA::pc;
    c.aluSrcB = cpu::ALUSrcB::constant;
    c.aluOperation = cpu::ALUOperation::ADD;
    c.pcWrite = true;
    c.pcSource = cpu::PCSource::aluResult;
    return c;
}

cpu::MultiCycleControlSignals decode_cycle()
{
    cpu::MultiCycleControlSignals c;
    c.aWrite = true;
    c.bWrite = true;
    c.aluOutWrite = true;
    c.aluSrcA = cpu::ALUSrcA::pc;
    c.aluSrcB = cpu::ALUSrcB::sign_extend;
    c.aluOperation = cpu::ALUOperation::ADD;
    return c;
}

cpu::MultiCycleControlSignals rtype_execute(cpu::ALUOperation operation)
{
    cpu::MultiCycleControlSignals c;
    c.aluSrcA = cpu::ALUSrcA::RegA;
    c.aluSrcB = cpu::ALUSrcB::regB;
    c.aluOperation = operation;
    c.aluOutWrite = true;
    return c;
}

cpu::MultiCycleControlSignals itype_execute(cpu::ALUOperation operation)
{
    cpu::MultiCycleControlSignals c;
    c.aluSrcA = cpu::ALUSrcA::RegA;
    c.aluSrcB = cpu::ALUSrcB::sign_extend;
    c.aluOperation = operation;
    c.aluOutWrite = true;
    return c;
}

cpu::MultiCycleControlSignals alu_writeback()
{
    cpu::MultiCycleControlSignals c;
    c.regWrite = true;
    c.regDst = true;
    c.writebackSource = cpu::WriteBackSource::aluOut;
    return c;
}

cpu::MultiCycleControlSignals memory_address_cycle()
{
    cpu::MultiCycleControlSignals c;
    c.aluSrcA = cpu::ALUSrcA::RegA;
    c.aluSrcB = cpu::ALUSrcB::sign_extend;
    c.aluOperation = cpu::ALUOperation::ADD;
    c.aluOutWrite = true;
    return c;
}

cpu::MultiCycleControlSignals memory_read_cycle()
{
    cpu::MultiCycleControlSignals c;
    c.memRead = true;
    c.mdrWrite = true;
    c.iorD = true;
    return c;
}

cpu::MultiCycleControlSignals memory_write_cycle()
{
    cpu::MultiCycleControlSignals c;
    c.memWrite = true;
    c.iorD = true;
    return c;
}

cpu::MultiCycleControlSignals memory_writeback()
{
    cpu::MultiCycleControlSignals c;
    c.regWrite = true;
    c.regDst = true;
    c.writebackSource = cpu::WriteBackSource::memoryData;
    return c;
}

cpu::MultiCycleControlSignals jump_cycle()
{
    cpu::MultiCycleControlSignals c;
    c.pcWrite = true;
    c.pcSource = cpu::PCSource::jumpTarget;
    return c;
}

void test_fetch_and_pc_increment()
{
    std::cout << "Testing fetch cycle and PC increment...\n";

    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    const std::uint32_t addi =
        encode_i_type(cpu::Opcode::ADDI, cpu::Register::R1, cpu::Register::R0, 5);
    datapath.load_instructions({addi});

    datapath.evaluate(fetch_cycle());
    assert(datapath.memory_read_data().read_value() == addi);
    assert(datapath.src_a().read_value() == 0);
    assert(datapath.src_b().read_value() == 1);
    assert(datapath.alu_result().read_value() == 1);

    datapath.step();
    assert(datapath.instruction().read_value() == addi);
    assert(datapath.pc().read_value() == 1);
    std::cout << "  [PASS] fetch latches IR and writes PC+1\n";
}

void test_add_instruction()
{
    std::cout << "Testing multi-cycle ADD...\n";

    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    const std::uint32_t add = encode_r_type(
        cpu::Opcode::ADD,
        cpu::Register::R3,
        cpu::Register::R1,
        cpu::Register::R2
    );
    datapath.load_instructions({add});
    datapath.write_register_for_test(cpu::Register::R1, 10);
    datapath.write_register_for_test(cpu::Register::R2, 20);

    datapath.step(fetch_cycle());
    assert(datapath.instruction().read_value() == add);

    datapath.step(decode_cycle());
    assert(datapath.decoded_instruction().opcode == cpu::Opcode::ADD);
    assert(datapath.decoded_instruction().rd == cpu::Register::R3);
    assert(datapath.operand_a().read_value() == 10);
    assert(datapath.operand_b().read_value() == 20);

    datapath.step(rtype_execute(cpu::ALUOperation::ADD));
    assert(datapath.alu_result().read_value() == 30);
    assert(datapath.alu_out().read_value() == 30);

    datapath.step(alu_writeback());
    assert(datapath.read_register_for_test(cpu::Register::R3) == 30);
    std::cout << "  [PASS] ADD R3, R1, R2 writes 30 to R3\n";
}

void test_addi_instruction()
{
    std::cout << "Testing multi-cycle ADDI...\n";

    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    const std::uint32_t addi =
        encode_i_type(cpu::Opcode::ADDI, cpu::Register::R2, cpu::Register::R1, 5);
    datapath.load_instructions({addi});
    datapath.write_register_for_test(cpu::Register::R1, 7);

    datapath.step(fetch_cycle());
    datapath.step(decode_cycle());
    assert(datapath.decoded_instruction().opcode == cpu::Opcode::ADDI);
    assert(datapath.immediate().read_value() == 5);

    datapath.step(itype_execute(cpu::ALUOperation::ADD));
    assert(datapath.alu_out().read_value() == 12);

    datapath.step(alu_writeback());
    assert(datapath.read_register_for_test(cpu::Register::R2) == 12);
    std::cout << "  [PASS] ADDI R2, R1, 5 writes 12 to R2\n";
}

void test_sw_then_lw()
{
    std::cout << "Testing multi-cycle SW then LW...\n";

    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    constexpr std::uint32_t ram_address = 0x84;
    const std::uint32_t sw =
        encode_s_type(cpu::Opcode::SW, cpu::Register::R2, cpu::Register::R1, 0);
    const std::uint32_t lw =
        encode_i_type(cpu::Opcode::LW, cpu::Register::R3, cpu::Register::R1, 0);
    datapath.load_instructions({sw, lw});
    datapath.write_register_for_test(cpu::Register::R1, ram_address);
    datapath.write_register_for_test(cpu::Register::R2, 42);

    datapath.step(fetch_cycle());
    datapath.step(decode_cycle());
    datapath.step(memory_address_cycle());
    assert(datapath.alu_out().read_value() == ram_address);

    datapath.evaluate(memory_write_cycle());
    assert(datapath.memory_address().read_value() == ram_address);
    datapath.step();

    datapath.step(fetch_cycle());
    datapath.step(decode_cycle());
    datapath.step(memory_address_cycle());
    datapath.step(memory_read_cycle());
    assert(datapath.memory_read_data().read_value() == 42);

    datapath.step(memory_writeback());
    assert(datapath.read_register_for_test(cpu::Register::R3) == 42);
    std::cout << "  [PASS] SW stores 42 and LW writes it back to R3\n";
}

void test_jump()
{
    std::cout << "Testing multi-cycle J...\n";

    logic::Clock clock;
    logic::Wire reset(logic::LogicState::LOW);
    Datapath datapath(clock, reset);

    const std::uint32_t jump = encode_j_type(cpu::Opcode::J, 4);
    datapath.load_instructions({jump});

    datapath.step(fetch_cycle());
    assert(datapath.pc().read_value() == 1);

    datapath.step(decode_cycle());
    datapath.step(jump_cycle());
    assert(datapath.pc().read_value() == 4);
    std::cout << "  [PASS] J writes the jump target into PC\n";
}

} // namespace

int main()
{
    std::cout << "=== Running Multi-Cycle Datapath Tests ===\n";
    test_fetch_and_pc_increment();
    test_add_instruction();
    test_addi_instruction();
    test_sw_then_lw();
    test_jump();
    std::cout << "[PASS] All Multi-Cycle Datapath Tests Passed!\n";
    return 0;
}
