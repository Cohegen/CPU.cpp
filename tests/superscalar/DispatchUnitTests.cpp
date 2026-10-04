#include "../../superscalar/core/dispatch/DispatchUnit.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/issue/IssueQueue.hpp"
#include "../../superscalar/core/issue/PhysicalRegisterFile.hpp"
#include "../../superscalar/core/rename/RenameUnit.hpp"
#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#include "../../superscalar/core/rename/RenameBundle.hpp"
#include "../../superscalar/core/decode/DecodeBundle.hpp"
#include "../../superscalar/core/decode/DecodeTypes.hpp"
#include "../../include/isa/Opcode.hpp"
#include "../../include/isa/Registers.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

cpu::DecodeBundle make_decode(
    bool valid,
    cpu::Opcode op,
    cpu::Register rd,
    cpu::Register rs1,
    cpu::Register rs2,
    bool register_write,
    cpu::ALUOperation alu,
    cpu::OperandSource operand_a,
    cpu::OperandSource operand_b,
    std::uint32_t pc,
    std::int32_t imm = 0
)
{
    cpu::DecodeBundle b{};
    b.valid = valid;
    b.pc = pc;
    b.instruction = 0xA5A5A5A5;
    b.opcode = op;
    b.rd = rd;
    b.rs1 = rs1;
    b.rs2 = rs2;
    b.immediate = imm;
    b.register_write = register_write;
    b.alu_operation = alu;
    b.operand_a = operand_a;
    b.operand_b = operand_b;
    return b;
}

cpu::DecodeBundle make_add(cpu::Register rd, cpu::Register rs1, cpu::Register rs2, std::uint32_t pc)
{
    return make_decode(
        true, cpu::Opcode::ADD, rd, rs1, rs2, true,
        cpu::ALUOperation::ADD,
        cpu::OperandSource::REGISTER,
        cpu::OperandSource::REGISTER,
        pc
    );
}

cpu::DecodeBundle make_addi(cpu::Register rd, cpu::Register rs1, std::int32_t imm, std::uint32_t pc)
{
    return make_decode(
        true, cpu::Opcode::ADDI, rd, rs1, cpu::Register::R0, true,
        cpu::ALUOperation::ADD,
        cpu::OperandSource::REGISTER,
        cpu::OperandSource::IMMEDIATE,
        pc, imm
    );
}

cpu::DecodeBundle make_li(cpu::Register rd, std::int32_t imm, std::uint32_t pc)
{
    return make_decode(
        true, cpu::Opcode::LI, rd, cpu::Register::R0, cpu::Register::R0, true,
        cpu::ALUOperation::NONE,
        cpu::OperandSource::NONE,
        cpu::OperandSource::IMMEDIATE,
        pc, imm
    );
}

cpu::DecodeBundle make_not(cpu::Register rd, cpu::Register rs1, std::uint32_t pc)
{
    return make_decode(
        true, cpu::Opcode::NOT, rd, rs1, cpu::Register::R0, true,
        cpu::ALUOperation::NOT,
        cpu::OperandSource::REGISTER,
        cpu::OperandSource::NONE,
        pc
    );
}

template<std::size_t Capacity>
const cpu::IssueQueueEntry* find_iq(const cpu::IssueQueue<Capacity>& queue, std::uint32_t pc)
{
    for (std::size_t i = 0; i < queue.capacity(); ++i)
    {
        const cpu::IssueQueueEntry* entry = queue.entry(i);
        if (entry != nullptr && entry->valid && entry->pc == pc)
        {
            return entry;
        }
    }
    return nullptr;
}

struct Backend
{
    cpu::RegisterAliasTable rat{64};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::PhysicalRegisterFile<64> prf{};
    cpu::ReOrderBuffer<16> rob{};
    cpu::IssueQueue<16> iq{};
    cpu::RenameUnit<64> rename;
    cpu::DispatchUnit<16, 16, 64> dispatch;

    Backend()
        : rename(rat, free_list, prf),
          dispatch(rob, iq, prf)
    {
        prf.reset();
        for (std::uint8_t p = 0; p < 16; ++p)
        {
            prf.write(p, static_cast<std::uint32_t>(p) * 10u);
        }
    }
};

void test_single_instruction_dispatch()
{
    Backend b;

    const auto decoded = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0x1000);
    const cpu::RenameBundle renamed = b.rename.rename(decoded);

    std::size_t rob_index = 999;
    assert(b.dispatch.dispatch(renamed, rob_index));
    assert(rob_index == 0);
    assert(b.rob.size() == 1);
    assert(b.iq.size() == 1);

    const auto* rob_entry = b.rob.entry(rob_index);
    assert(rob_entry != nullptr);
    assert(rob_entry->valid);
    assert(rob_entry->physical_rd == renamed.physical_rd);

    const auto* iq_entry = find_iq(b.iq, 0x1000);
    assert(iq_entry != nullptr);
    assert(iq_entry->rob_index == rob_index);
    assert(iq_entry->physical_rd == renamed.physical_rd);
    assert(iq_entry->physical_rs1 == renamed.physical_rs1);
    assert(iq_entry->physical_rs2 == renamed.physical_rs2);
    assert(iq_entry->alu_operation == cpu::ALUOperation::ADD);
    assert(iq_entry->operand_a == cpu::OperandSource::REGISTER);
    assert(iq_entry->operand_b == cpu::OperandSource::REGISTER);

    std::cout << "  [PASS] single instruction dispatch\n";
}

void test_two_wide_dispatch()
{
    Backend b;

    const auto d0 = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0x1000);
    const auto d1 = make_add(cpu::Register::R4, cpu::Register::R5, cpu::Register::R6, 0x1004);
    cpu::RenameBundle r0, r1;
    b.rename.rename(d0, d1, r0, r1);

    std::size_t i0 = 0, i1 = 0;
    assert(b.dispatch.dispatch(r0, r1, i0, i1));
    assert(b.rob.size() == 2);
    assert(b.iq.size() == 2);
    assert(i0 == 0);
    assert(i1 == 1);

    assert(find_iq(b.iq, 0x1000) != nullptr);
    assert(find_iq(b.iq, 0x1004) != nullptr);

    std::cout << "  [PASS] two-wide dispatch\n";
}

void test_rob_indices_preserved()
{
    Backend b;

    const auto d0 = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0x2000);
    const auto d1 = make_addi(cpu::Register::R4, cpu::Register::R5, 7, 0x2004);
    cpu::RenameBundle r0, r1;
    b.rename.rename(d0, d1, r0, r1);

    std::size_t i0 = 0, i1 = 0;
    assert(b.dispatch.dispatch(r0, r1, i0, i1));

    const auto* iq0 = find_iq(b.iq, 0x2000);
    const auto* iq1 = find_iq(b.iq, 0x2004);
    assert(iq0 != nullptr);
    assert(iq1 != nullptr);
    assert(iq0->rob_index == i0);
    assert(iq1->rob_index == i1);
    assert(b.rob.entry(i0)->pc == 0x2000);
    assert(b.rob.entry(i1)->pc == 0x2004);

    std::cout << "  [PASS] ROB indices preserved\n";
}

void test_source_operands_read_from_prf()
{
    Backend b;
    b.prf.write(2, 111);
    b.prf.write(3, 222);

    const auto decoded = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0x3000);
    const cpu::RenameBundle renamed = b.rename.rename(decoded);
    assert(renamed.physical_rs1 == 2);
    assert(renamed.physical_rs2 == 3);

    std::size_t rob_index = 0;
    assert(b.dispatch.dispatch(renamed, rob_index));

    const auto* iq_entry = find_iq(b.iq, 0x3000);
    assert(iq_entry != nullptr);
    assert(iq_entry->rs1_value == 111);
    assert(iq_entry->rs2_value == 222);
    assert(iq_entry->immediate == 0);

    std::cout << "  [PASS] source operands read from PRF\n";
}

void test_ready_source_immediately_issuable()
{
    Backend b;

    const auto decoded = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0x4000);
    const cpu::RenameBundle renamed = b.rename.rename(decoded);

    std::size_t rob_index = 0;
    assert(b.dispatch.dispatch(renamed, rob_index));

    const auto* selected = b.iq.select_ready();
    assert(selected != nullptr);
    assert(selected->pc == 0x4000);
    assert(selected->rs1_ready);
    assert(selected->rs2_ready);

    std::cout << "  [PASS] ready source -> immediately issuable\n";
}

void test_unready_source_waits()
{
    Backend b;

    const auto producer = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0x5000);
    const cpu::RenameBundle renamed_producer = b.rename.rename(producer);
    std::size_t producer_rob = 0;
    assert(b.dispatch.dispatch(renamed_producer, producer_rob));
    b.iq.mark_issued(b.iq.select_ready());

    const auto consumer = make_add(cpu::Register::R4, cpu::Register::R1, cpu::Register::R5, 0x5004);
    const cpu::RenameBundle renamed_consumer = b.rename.rename(consumer);
    assert(renamed_consumer.physical_rs1 == renamed_producer.physical_rd);
    assert(!b.prf.ready(renamed_consumer.physical_rs1));

    std::size_t consumer_rob = 0;
    assert(b.dispatch.dispatch(renamed_consumer, consumer_rob));

    assert(b.iq.select_ready() == nullptr);
    const auto* waiting = find_iq(b.iq, 0x5004);
    assert(waiting != nullptr);
    assert(!waiting->rs1_ready);
    assert(waiting->rs2_ready);

    std::cout << "  [PASS] unready source -> waits\n";
}

void test_li_does_not_wait_for_rs1()
{
    Backend b;
    b.prf.allocate(0);
    assert(!b.prf.ready(0));

    const auto decoded = make_li(cpu::Register::R1, 42, 0x6000);
    const cpu::RenameBundle renamed = b.rename.rename(decoded);
    assert(renamed.operand_a == cpu::OperandSource::NONE);
    assert(renamed.operand_b == cpu::OperandSource::IMMEDIATE);
    assert(renamed.immediate == 42);

    std::size_t rob_index = 0;
    assert(b.dispatch.dispatch(renamed, rob_index));

    const auto* iq_entry = find_iq(b.iq, 0x6000);
    assert(iq_entry != nullptr);
    assert(iq_entry->rs1_ready);
    assert(iq_entry->rs2_ready);
    assert(iq_entry->immediate == 42);
    assert(b.iq.select_ready() == iq_entry);

    std::cout << "  [PASS] LI doesn't wait for rs1\n";
}

void test_not_does_not_wait_for_rs2()
{
    Backend b;
    b.prf.allocate(0);
    assert(!b.prf.ready(0));

    const auto decoded = make_not(cpu::Register::R1, cpu::Register::R2, 0x7000);
    const cpu::RenameBundle renamed = b.rename.rename(decoded);
    assert(renamed.operand_a == cpu::OperandSource::REGISTER);
    assert(renamed.operand_b == cpu::OperandSource::NONE);

    std::size_t rob_index = 0;
    assert(b.dispatch.dispatch(renamed, rob_index));

    const auto* iq_entry = find_iq(b.iq, 0x7000);
    assert(iq_entry != nullptr);
    assert(iq_entry->rs1_ready);
    assert(iq_entry->rs2_ready);
    assert(iq_entry->rs1_value == 20);
    assert(b.iq.select_ready() == iq_entry);

    std::cout << "  [PASS] NOT doesn't wait for rs2\n";
}

void test_lane1_sees_lane0_renamed_destination()
{
    Backend b;

    const auto d0 = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0x8000);
    const auto d1 = make_addi(cpu::Register::R4, cpu::Register::R1, 1, 0x8004);
    cpu::RenameBundle r0, r1;
    b.rename.rename(d0, d1, r0, r1);

    assert(r1.physical_rs1 == r0.physical_rd);
    assert(!b.prf.ready(r0.physical_rd));

    std::size_t i0 = 0, i1 = 0;
    assert(b.dispatch.dispatch(r0, r1, i0, i1));

    const auto* iq0 = find_iq(b.iq, 0x8000);
    const auto* iq1 = find_iq(b.iq, 0x8004);
    assert(iq0 != nullptr);
    assert(iq1 != nullptr);
    assert(iq1->physical_rs1 == iq0->physical_rd);
    assert(iq0->rs1_ready && iq0->rs2_ready);
    assert(!iq1->rs1_ready);
    assert(iq1->rs2_ready);

    const auto* selected = b.iq.select_ready();
    assert(selected != nullptr);
    assert(selected->pc == 0x8000);

    std::cout << "  [PASS] lane 1 sees lane 0's renamed destination\n";
}

void test_rob_full_stalls_dispatch()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::PhysicalRegisterFile<64> prf;
    prf.reset();
    prf.write(2, 20);
    prf.write(3, 30);

    cpu::ReOrderBuffer<2> rob;
    cpu::IssueQueue<16> iq;
    cpu::RenameUnit rename(rat, free_list, prf);
    cpu::DispatchUnit<2, 16, 64> dispatch(rob, iq, prf);

    const auto d0 = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0x9000);
    const auto d1 = make_add(cpu::Register::R4, cpu::Register::R2, cpu::Register::R3, 0x9004);
    cpu::RenameBundle r0, r1;
    rename.rename(d0, d1, r0, r1);

    std::size_t i0 = 0, i1 = 0;
    assert(dispatch.dispatch(r0, r1, i0, i1));
    assert(rob.full());
    assert(iq.size() == 2);

    const auto extra_decode = make_add(cpu::Register::R5, cpu::Register::R2, cpu::Register::R3, 0x9008);
    const cpu::RenameBundle extra = rename.rename(extra_decode);
    std::size_t extra_rob = 0;
    assert(!dispatch.dispatch(extra, extra_rob));
    assert(rob.size() == 2);
    assert(iq.size() == 2);
    assert(find_iq(iq, 0x9008) == nullptr);

    std::cout << "  [PASS] ROB full -> dispatch stalls\n";
}

void test_issue_queue_full_stalls_dispatch()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::PhysicalRegisterFile<64> prf;
    prf.reset();
    prf.write(2, 20);
    prf.write(3, 30);

    cpu::ReOrderBuffer<16> rob;
    cpu::IssueQueue<2> iq;
    cpu::RenameUnit rename(rat, free_list, prf);
    cpu::DispatchUnit<16, 2, 64> dispatch(rob, iq, prf);

    const auto d0 = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0xA000);
    const auto d1 = make_add(cpu::Register::R4, cpu::Register::R2, cpu::Register::R3, 0xA004);
    cpu::RenameBundle r0, r1;
    rename.rename(d0, d1, r0, r1);

    std::size_t i0 = 0, i1 = 0;
    assert(dispatch.dispatch(r0, r1, i0, i1));
    assert(iq.full());
    assert(rob.size() == 2);

    const auto extra_decode = make_add(cpu::Register::R5, cpu::Register::R2, cpu::Register::R3, 0xA008);
    const cpu::RenameBundle extra = rename.rename(extra_decode);
    std::size_t extra_rob = 0;
    assert(!dispatch.dispatch(extra, extra_rob));
    assert(rob.size() == 2);
    assert(iq.size() == 2);
    assert(rob.entry(2) == nullptr || !rob.entry(2)->valid);

    std::cout << "  [PASS] Issue Queue full -> dispatch stalls\n";
}

void test_two_instructions_dispatch_atomically()
{
    cpu::RegisterAliasTable rat(64);
    cpu::PhysicalRegisterFreeList free_list(64, 16);
    cpu::PhysicalRegisterFile<64> prf;
    prf.reset();
    prf.write(2, 20);
    prf.write(3, 30);
    prf.write(5, 50);
    prf.write(6, 60);

    cpu::ReOrderBuffer<2> rob;
    cpu::IssueQueue<16> iq;
    cpu::RenameUnit rename(rat, free_list, prf);
    cpu::DispatchUnit<2, 16, 64> dispatch(rob, iq, prf);

    const auto filler_decode = make_add(cpu::Register::R1, cpu::Register::R2, cpu::Register::R3, 0xB000);
    const cpu::RenameBundle filler = rename.rename(filler_decode);
    std::size_t filler_rob = 0;
    assert(dispatch.dispatch(filler, filler_rob));
    assert(rob.size() == 1);
    assert(iq.size() == 1);

    const auto d0 = make_add(cpu::Register::R4, cpu::Register::R5, cpu::Register::R6, 0xB004);
    const auto d1 = make_add(cpu::Register::R7, cpu::Register::R5, cpu::Register::R6, 0xB008);
    cpu::RenameBundle r0, r1;
    rename.rename(d0, d1, r0, r1);

    std::size_t i0 = 0, i1 = 0;
    assert(!dispatch.dispatch(r0, r1, i0, i1));
    assert(rob.size() == 1);
    assert(iq.size() == 1);
    assert(find_iq(iq, 0xB004) == nullptr);
    assert(find_iq(iq, 0xB008) == nullptr);

    std::cout << "  [PASS] two instructions dispatch atomically\n";
}

} // namespace

int main()
{
    std::cout << "=========================================================\n";
    std::cout << "--- Testing Dispatch (ROB + Issue Queue + PRF)        ---\n";
    std::cout << "=========================================================\n";

    test_single_instruction_dispatch();
    test_two_wide_dispatch();
    test_rob_indices_preserved();
    test_source_operands_read_from_prf();
    test_ready_source_immediately_issuable();
    test_unready_source_waits();
    test_li_does_not_wait_for_rs1();
    test_not_does_not_wait_for_rs2();
    test_lane1_sees_lane0_renamed_destination();
    test_rob_full_stalls_dispatch();
    test_issue_queue_full_stalls_dispatch();
    test_two_instructions_dispatch_atomically();

    std::cout << "\n[PASS] All 12 Dispatch tests passed successfully!\n";
    return 0;
}
