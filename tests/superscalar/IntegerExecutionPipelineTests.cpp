#include "../../superscalar/core/decode/DecodeBundle.hpp"
#include "../../superscalar/core/dispatch/DispatchUnit.hpp"
#include "../../superscalar/core/dispatch/ReOrderBuffer.hpp"
#include "../../superscalar/core/execution/IntegerExecutionPipeline.hpp"
#include "../../superscalar/core/issue/IssueQueue.hpp"
#include "../../superscalar/core/issue/PhysicalRegisterFile.hpp"
#include "../../superscalar/core/rename/PhysicalRegisterFreeList.hpp"
#include "../../superscalar/core/rename/RegisterAliasTable.hpp"
#include "../../superscalar/core/rename/RenameUnit.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace
{
cpu::DecodeBundle immediate(cpu::Register destination, std::int32_t value)
{
    cpu::DecodeBundle bundle{};
    bundle.valid = true;
    bundle.rd = destination;
    bundle.immediate = value;
    bundle.alu_operation = cpu::ALUOperation::ADD;
    bundle.operand_a = cpu::OperandSource::NONE;
    bundle.operand_b = cpu::OperandSource::IMMEDIATE;
    bundle.register_write = true;
    return bundle;
}

cpu::DecodeBundle add(cpu::Register destination, cpu::Register lhs, cpu::Register rhs)
{
    cpu::DecodeBundle bundle{};
    bundle.valid = true;
    bundle.rd = destination;
    bundle.rs1 = lhs;
    bundle.rs2 = rhs;
    bundle.alu_operation = cpu::ALUOperation::ADD;
    bundle.operand_a = cpu::OperandSource::REGISTER;
    bundle.operand_b = cpu::OperandSource::REGISTER;
    bundle.register_write = true;
    return bundle;
}
}

int main()
{
    cpu::RegisterAliasTable rat{64};
    cpu::PhysicalRegisterFreeList free_list{64, 16};
    cpu::PhysicalRegisterFile<64> prf{};
    cpu::RenameUnit<64> rename{rat, free_list, prf};
    cpu::ReOrderBuffer<16> rob{};
    cpu::IssueQueue<16> issue_queue{};
    cpu::DispatchUnit<16, 16, 64> dispatch{rob, issue_queue, prf};
    cpu::IntegerExecutionPipeline<16, 16, 64> pipeline{rob, issue_queue, prf, free_list};

    // r1 = 10 and r2 = 20 are independent and must issue together.
    cpu::RenameBundle first{};
    cpu::RenameBundle second{};
    rename.rename(immediate(cpu::Register::R1, 10), immediate(cpu::Register::R2, 20), first, second);
    assert(first.valid && second.valid);
    const auto old_r1 = first.old_physical_rd;
    const auto old_r2 = second.old_physical_rd;
    std::size_t first_rob = 0;
    std::size_t second_rob = 0;
    assert(dispatch.dispatch(first, second, first_rob, second_rob));

    // Dispatch a dependent instruction before its producers execute.  It must
    // remain in the issue queue until write-back broadcasts both values.
    const cpu::RenameBundle dependent = rename.rename(add(cpu::Register::R3, cpu::Register::R1, cpu::Register::R2));
    assert(dependent.valid);
    std::size_t dependent_rob = 0;
    assert(dispatch.dispatch(dependent, dependent_rob));
    assert(!prf.ready(dependent.physical_rs1));
    assert(!prf.ready(dependent.physical_rs2));

    const auto first_cycle = pipeline.cycle();
    assert(first_cycle.issued == 2);
    assert(first_cycle.retired == 2);
    assert(prf.read(first.physical_rd) == 10);
    assert(prf.read(second.physical_rd) == 20);
    assert(prf.ready(first.physical_rd) && prf.ready(second.physical_rd));
    assert(free_list.size() == 47); // Two retired mappings returned; r3 remains in flight.
    assert(issue_queue.size() == 1);

    // r3 = r1 + r2 was woken by the two write-back broadcasts.
    const auto second_cycle = pipeline.cycle();
    assert(second_cycle.issued == 1);
    assert(second_cycle.retired == 1);
    assert(prf.ready(dependent.physical_rd));
    assert(prf.read(dependent.physical_rd) == 30);
    assert(rob.empty() && issue_queue.empty());
    assert(free_list.size() == 48);

    // The old mappings were retained until retirement, never released at rename.
    assert(old_r1 == static_cast<cpu::RenameBundle::PhysicalRegister>(cpu::Register::R1));
    assert(old_r2 == static_cast<cpu::RenameBundle::PhysicalRegister>(cpu::Register::R2));
    std::cout << "[PASS] Two-wide integer issue, write-back, wake-up, and ordered retirement.\n";
    return 0;
}
