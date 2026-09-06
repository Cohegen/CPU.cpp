#include <cassert>
#include <iostream>
#include <cstdint>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>

#include "../../pipelined_cpu/core/ControlSignals.hpp"
#include "../../pipelined_cpu/registers/IF_ID.hpp"
#include "../../pipelined_cpu/registers/ID_EX.hpp"
#include "../../pipelined_cpu/registers/EX_MEM.hpp"
#include "../../pipelined_cpu/registers/MEM_WB.hpp"

// Helper function to clock any component
template <typename TComponent>
void clock_component(logic::Wire& clock, TComponent& comp)
{
    // Falling edge / setup
    clock.write(logic::LogicState::LOW);
    comp.evaluate();

    // Rising edge / capture
    clock.write(logic::LogicState::HIGH);
    comp.evaluate();
}


// Test 1: Basic pass-through / capture test

void test_1_basic_passthrough_capture()
{
    std::cout << "[Test 1] Running Basic Pass-Through / Capture Test...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    // 1. IF_ID
    {
        logic::Bus<32> instr_in;
        logic::Bus<32> pcplus4_in;
        cpu::IF_ID<32, 32> if_id(instr_in, pcplus4_in, clock, reset, enable);

        instr_in.write_value(0x12345678);
        pcplus4_in.write_value(100);

        clock_component(clock, if_id);

        assert(if_id.instruction().read_value() == 0x12345678);
        assert(if_id.pcplus4().read_value() == 100);
    }

    // 2. ID_EX
    {
        logic::Bus<32> pcplus4_in;
        logic::Bus<32> rd1_in;
        logic::Bus<32> rd2_in;
        logic::Bus<32> imm_in;
        logic::Bus<5> rs_in;
        logic::Bus<5> rt_in;
        logic::Bus<5> rd_in;

        cpu::ID_EX<32, 32, 5> id_ex(
            pcplus4_in, rd1_in, rd2_in, imm_in, rs_in, rt_in, rd_in,
            clock, reset, enable
        );

        pcplus4_in.write_value(100);
        rd1_in.write_value(25);
        rd2_in.write_value(40);
        imm_in.write_value(12);
        rs_in.write_value(1);
        rt_in.write_value(2);
        rd_in.write_value(3);

        clock_component(clock, id_ex);

        assert(id_ex.pcplus4().read_value() == 100);
        assert(id_ex.read_data1().read_value() == 25);
        assert(id_ex.read_data2().read_value() == 40);
        assert(id_ex.immediate().read_value() == 12);
        assert(id_ex.rs().read_value() == 1);
        assert(id_ex.rt().read_value() == 2);
        assert(id_ex.rd().read_value() == 3);
    }

    // 3. EX_MEM
    {
        logic::Bus<32> alu_in;
        logic::Bus<32> rd2_in;
        logic::Bus<5> rd_in;

        cpu::EX_MEM<32, 5> ex_mem(alu_in, rd2_in, rd_in, clock, reset, enable);

        alu_in.write_value(123);
        rd2_in.write_value(40);
        rd_in.write_value(3);

        clock_component(clock, ex_mem);

        assert(ex_mem.alu_result().read_value() == 123);
        assert(ex_mem.read_data2().read_value() == 40);
        assert(ex_mem.rd().read_value() == 3);
    }

    // 4. MEM_WB
    {
        logic::Bus<32> read_data_in;
        logic::Bus<32> alu_in;
        logic::Bus<5> rd_in;

        cpu::MEM_WB<32, 5> mem_wb(read_data_in, alu_in, rd_in, clock, reset, enable);

        read_data_in.write_value(99);
        alu_in.write_value(123);
        rd_in.write_value(3);

        clock_component(clock, mem_wb);

        assert(mem_wb.read_data().read_value() == 99);
        assert(mem_wb.alu_result().read_value() == 123);
        assert(mem_wb.rd().read_value() == 3);
    }

    std::cout << "  [PASS] Test 1 passed successfully.\n";
}


// Test 2: Reset test

void test_2_reset()
{
    std::cout << "[Test 2] Running Reset Test...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    // 1. IF_ID
    {
        logic::Bus<32> instr_in;
        logic::Bus<32> pcplus4_in;
        cpu::IF_ID<32, 32> if_id(instr_in, pcplus4_in, clock, reset, enable);

        instr_in.write_value(0x87654321);
        pcplus4_in.write_value(200);
        clock_component(clock, if_id);
        assert(if_id.instruction().read_value() == 0x87654321);
        assert(if_id.pcplus4().read_value() == 200);

        // Assert reset
        reset.write(logic::LogicState::HIGH);
        clock_component(clock, if_id);

        assert(if_id.instruction().read_value() == 0);
        assert(if_id.pcplus4().read_value() == 0);
        reset.write(logic::LogicState::LOW);
    }

    // 2. ID_EX
    {
        logic::Bus<32> pcplus4_in;
        logic::Bus<32> rd1_in;
        logic::Bus<32> rd2_in;
        logic::Bus<32> imm_in;
        logic::Bus<5> rs_in;
        logic::Bus<5> rt_in;
        logic::Bus<5> rd_in;

        cpu::ID_EX<32, 32, 5> id_ex(
            pcplus4_in, rd1_in, rd2_in, imm_in, rs_in, rt_in, rd_in,
            clock, reset, enable
        );

        pcplus4_in.write_value(100);
        rd1_in.write_value(25);
        rd2_in.write_value(40);
        imm_in.write_value(12);
        rs_in.write_value(1);
        rt_in.write_value(2);
        rd_in.write_value(3);
        id_ex.set_reg_write(true);

        clock_component(clock, id_ex);
        assert(id_ex.pcplus4().read_value() == 100);
        assert(id_ex.reg_write() == true);

        // Assert reset
        reset.write(logic::LogicState::HIGH);
        clock_component(clock, id_ex);

        assert(id_ex.pcplus4().read_value() == 0);
        assert(id_ex.read_data1().read_value() == 0);
        assert(id_ex.read_data2().read_value() == 0);
        assert(id_ex.immediate().read_value() == 0);
        assert(id_ex.rs().read_value() == 0);
        assert(id_ex.rt().read_value() == 0);
        assert(id_ex.rd().read_value() == 0);
        assert(id_ex.reg_write() == false);
        reset.write(logic::LogicState::LOW);
    }

    // 3. EX_MEM: Specifically tested ALUResult = 123, Rd = 7, RegWrite = 1 -> reset -> all 0
    {
        logic::Bus<32> alu_in;
        logic::Bus<32> rd2_in;
        logic::Bus<5> rd_in;

        cpu::EX_MEM<32, 5> ex_mem(alu_in, rd2_in, rd_in, clock, reset, enable);

        alu_in.write_value(123);
        rd_in.write_value(7);
        rd2_in.write_value(55);
        ex_mem.set_reg_write(true);

        clock_component(clock, ex_mem);
        assert(ex_mem.alu_result().read_value() == 123);
        assert(ex_mem.rd().read_value() == 7);
        assert(ex_mem.reg_write() == true);

        // Assert reset
        reset.write(logic::LogicState::HIGH);
        clock_component(clock, ex_mem);

        assert(ex_mem.alu_result().read_value() == 0);
        assert(ex_mem.rd().read_value() == 0);
        assert(ex_mem.read_data2().read_value() == 0);
        assert(ex_mem.reg_write() == false);
        reset.write(logic::LogicState::LOW);
    }

    // 4. MEM_WB
    {
        logic::Bus<32> read_data_in;
        logic::Bus<32> alu_in;
        logic::Bus<5> rd_in;

        cpu::MEM_WB<32, 5> mem_wb(read_data_in, alu_in, rd_in, clock, reset, enable);

        read_data_in.write_value(77);
        alu_in.write_value(123);
        rd_in.write_value(7);
        mem_wb.set_reg_write(true);

        clock_component(clock, mem_wb);
        assert(mem_wb.alu_result().read_value() == 123);
        assert(mem_wb.rd().read_value() == 7);
        assert(mem_wb.reg_write() == true);

        // Assert reset
        reset.write(logic::LogicState::HIGH);
        clock_component(clock, mem_wb);

        assert(mem_wb.read_data().read_value() == 0);
        assert(mem_wb.alu_result().read_value() == 0);
        assert(mem_wb.rd().read_value() == 0);
        assert(mem_wb.reg_write() == false);
        reset.write(logic::LogicState::LOW);
    }

    std::cout << "  [PASS] Test 2 passed successfully.\n";
}


// Test 3: Enable test (Pipeline stall)

void test_3_enable_stall()
{
    std::cout << "[Test 3] Running Enable Test (Pipeline Stall)...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    logic::Bus<32> alu_in;
    logic::Bus<32> rd2_in;
    logic::Bus<5> rd_in;

    cpu::EX_MEM<32, 5> ex_mem(alu_in, rd2_in, rd_in, clock, reset, enable);

    // Step 1: enable = 1, Load ALUResult = 100, Clock
    alu_in.write_value(100);
    enable.write(logic::LogicState::HIGH);
    clock_component(clock, ex_mem);

    assert(ex_mem.alu_result().read_value() == 100);

    // Step 2: change input ALUResult = 200, enable = 0, Clock again
    alu_in.write_value(200);
    enable.write(logic::LogicState::LOW);
    clock_component(clock, ex_mem);

    // Output should remain 100
    assert(ex_mem.alu_result().read_value() == 100);

    // Step 3: Clock again with enable = 0 and input = 300 -> stays 100
    alu_in.write_value(300);
    clock_component(clock, ex_mem);
    assert(ex_mem.alu_result().read_value() == 100);

    // Step 4: Re-enable (enable = 1) -> should capture 300
    enable.write(logic::LogicState::HIGH);
    clock_component(clock, ex_mem);
    assert(ex_mem.alu_result().read_value() == 300);

    std::cout << "  [PASS] Test 3 passed successfully.\n";
}


// Test 4: Reset priority test

void test_4_reset_priority()
{
    std::cout << "[Test 4] Running Reset Priority Test...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    logic::Bus<32> alu_in;
    logic::Bus<32> rd2_in;
    logic::Bus<5> rd_in;

    cpu::EX_MEM<32, 5> ex_mem(alu_in, rd2_in, rd_in, clock, reset, enable);

    // Case 1: Load non-zero value first
    alu_in.write_value(100);
    clock_component(clock, ex_mem);
    assert(ex_mem.alu_result().read_value() == 100);

    // Test reset = 1, enable = 0
    alu_in.write_value(200);
    reset.write(logic::LogicState::HIGH);
    enable.write(logic::LogicState::LOW);
    clock_component(clock, ex_mem);

    // Expected: reset wins, output = 0
    assert(ex_mem.alu_result().read_value() == 0);

    // Case 2: Load non-zero value again
    reset.write(logic::LogicState::LOW);
    enable.write(logic::LogicState::HIGH);
    alu_in.write_value(100);
    clock_component(clock, ex_mem);
    assert(ex_mem.alu_result().read_value() == 100);

    // Test reset = 1, enable = 1
    alu_in.write_value(200);
    reset.write(logic::LogicState::HIGH);
    enable.write(logic::LogicState::HIGH);
    clock_component(clock, ex_mem);

    // Expected: reset wins, output = 0
    assert(ex_mem.alu_result().read_value() == 0);

    std::cout << "  [PASS] Test 4 passed successfully.\n";
}


// Test 5: Clock edge test

void test_5_clock_edge()
{
    std::cout << "[Test 5] Running Clock Edge Test...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    logic::Bus<32> alu_in;
    logic::Bus<32> rd2_in;
    logic::Bus<5> rd_in;

    cpu::EX_MEM<32, 5> ex_mem(alu_in, rd2_in, rd_in, clock, reset, enable);

    // Initial state = 0
    ex_mem.evaluate();
    assert(ex_mem.alu_result().read_value() == 0);

    // Present ALUResult = 50 on input, but DO NOT clock yet
    alu_in.write_value(50);
    clock.write(logic::LogicState::LOW);
    ex_mem.evaluate();

    // Output must still contain old value (0)
    assert(ex_mem.alu_result().read_value() == 0);

    // Clock edge: clock transitions LOW -> HIGH
    clock.write(logic::LogicState::HIGH);
    ex_mem.evaluate();

    // Output now contains 50
    assert(ex_mem.alu_result().read_value() == 50);

    // Change input ALUResult = 100, but do NOT clock yet (clock stays HIGH)
    alu_in.write_value(100);
    ex_mem.evaluate();

    // Output must still remain 50!
    assert(ex_mem.alu_result().read_value() == 50);

    // Bring clock LOW (no rising edge)
    clock.write(logic::LogicState::LOW);
    ex_mem.evaluate();
    assert(ex_mem.alu_result().read_value() == 50);

    // Rising clock edge
    clock.write(logic::LogicState::HIGH);
    ex_mem.evaluate();

    // Output now updates to 100
    assert(ex_mem.alu_result().read_value() == 100);

    std::cout << "  [PASS] Test 5 passed successfully.\n";
}


// Test 6: Width tests

void test_6_widths()
{
    std::cout << "[Test 6] Running Width Tests...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    // 1. Rd width test: 5'b10101 (21 decimal)
    {
        logic::Bus<32> pcplus4_in;
        logic::Bus<32> rd1_in;
        logic::Bus<32> rd2_in;
        logic::Bus<32> imm_in;
        logic::Bus<5> rs_in;
        logic::Bus<5> rt_in;
        logic::Bus<5> rd_in;

        cpu::ID_EX<32, 32, 5> id_ex(
            pcplus4_in, rd1_in, rd2_in, imm_in, rs_in, rt_in, rd_in,
            clock, reset, enable
        );

        // 5'b10101 = 21
        rd_in.write_value(0b10101);
        rs_in.write_value(0b01010);
        rt_in.write_value(0b11111);

        clock_component(clock, id_ex);

        assert(id_ex.rd().read_value() == 0b10101);
        assert(id_ex.rs().read_value() == 0b01010);
        assert(id_ex.rt().read_value() == 0b11111);
    }

    // 2. Control width test: 1 bit
    {
        logic::Bus<32> pcplus4_in;
        logic::Bus<32> rd1_in;
        logic::Bus<32> rd2_in;
        logic::Bus<32> imm_in;
        logic::Bus<5> rs_in;
        logic::Bus<5> rt_in;
        logic::Bus<5> rd_in;

        cpu::ID_EX<32, 32, 5> id_ex(
            pcplus4_in, rd1_in, rd2_in, imm_in, rs_in, rt_in, rd_in,
            clock, reset, enable
        );

        id_ex.set_reg_write(true);
        id_ex.set_mem_read(false);
        clock_component(clock, id_ex);
        assert(id_ex.reg_write() == true);
        assert(id_ex.mem_read() == false);

        id_ex.set_reg_write(false);
        id_ex.set_mem_read(true);
        clock_component(clock, id_ex);
        assert(id_ex.reg_write() == false);
        assert(id_ex.mem_read() == true);
    }

    // 3. Custom template parameterization widths: ID_EX<16, 16, 4>
    {
        logic::Bus<16> pc16;
        logic::Bus<16> rd1_16;
        logic::Bus<16> rd2_16;
        logic::Bus<16> imm16;
        logic::Bus<4> rs4;
        logic::Bus<4> rt4;
        logic::Bus<4> rd4;

        cpu::ID_EX<16, 16, 4> id_ex_custom(
            pc16, rd1_16, rd2_16, imm16, rs4, rt4, rd4,
            clock, reset, enable
        );

        pc16.write_value(0x1234);
        rd1_16.write_value(0x5678);
        rd4.write_value(0b1101);

        clock_component(clock, id_ex_custom);

        assert(id_ex_custom.pcplus4().read_value() == 0x1234);
        assert(id_ex_custom.read_data1().read_value() == 0x5678);
        assert(id_ex_custom.rd().read_value() == 0b1101);
    }

    std::cout << "  [PASS] Test 6 passed successfully.\n";
}


// Test 7: Back-to-back instruction test

void test_7_back_to_back_instructions()
{
    std::cout << "[Test 7] Running Back-to-Back Instruction Test...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    // ID_EX inputs
    logic::Bus<32> id_pc;
    logic::Bus<32> id_rd1;
    logic::Bus<32> id_rd2;
    logic::Bus<32> id_imm;
    logic::Bus<5> id_rs;
    logic::Bus<5> id_rt;
    logic::Bus<5> id_rd;
    cpu::ID_EX<32, 32, 5> id_ex(id_pc, id_rd1, id_rd2, id_imm, id_rs, id_rt, id_rd, clock, reset, enable);

    // EX_MEM inputs (connected to ID_EX outputs / EX stage result)
    logic::Bus<32> ex_alu_result;
    cpu::EX_MEM<32, 5> ex_mem(ex_alu_result, id_ex.read_data2(), id_ex.rd(), clock, reset, enable);

    // MEM_WB inputs (connected to EX_MEM outputs / MEM stage read data)
    logic::Bus<32> mem_read_data;
    cpu::MEM_WB<32, 5> mem_wb(mem_read_data, ex_mem.alu_result(), ex_mem.rd(), clock, reset, enable);

    // Instruction definitions:
    // Inst 1: ADD R1, R2, R3 (tag: alu_res = 111, rd = 1)
    // Inst 2: SUB R4, R5, R6 (tag: alu_res = 222, rd = 4)
    // Inst 3: LW  R7, 0(R8)  (tag: alu_res = 333, mem_data = 777, rd = 7)

    // ------------------------------------------------------------------------
    // Cycle 1: Load Inst 1 into ID_EX
    // ------------------------------------------------------------------------
    id_rd1.write_value(10);
    id_rd2.write_value(20);
    id_rd.write_value(1);
    ex_alu_result.write_value(0);
    mem_read_data.write_value(0);

    // Clock all
    clock.write(logic::LogicState::LOW);
    id_ex.evaluate();
    ex_mem.evaluate();
    mem_wb.evaluate();

    clock.write(logic::LogicState::HIGH);
    id_ex.evaluate();
    ex_mem.evaluate();
    mem_wb.evaluate();

    assert(id_ex.rd().read_value() == 1);
    assert(ex_mem.rd().read_value() == 0); // empty
    assert(mem_wb.rd().read_value() == 0); // empty

    // ------------------------------------------------------------------------
    // Cycle 2: Inst 1 -> EX_MEM; Load Inst 2 into ID_EX
    // ------------------------------------------------------------------------
    ex_alu_result.write_value(111); // Inst 1 ALU result
    id_rd.write_value(4);           // Inst 2 into ID_EX

    clock.write(logic::LogicState::LOW);
    id_ex.evaluate();
    ex_mem.evaluate();
    mem_wb.evaluate();

    clock.write(logic::LogicState::HIGH);
    id_ex.evaluate();
    ex_mem.evaluate();
    mem_wb.evaluate();

    assert(id_ex.rd().read_value() == 4);           // Inst 2 in ID_EX
    assert(ex_mem.rd().read_value() == 1);          // Inst 1 in EX_MEM
    assert(ex_mem.alu_result().read_value() == 111);
    assert(mem_wb.rd().read_value() == 0);          // empty

    // ------------------------------------------------------------------------
    // Cycle 3: Inst 1 -> MEM_WB; Inst 2 -> EX_MEM; Load Inst 3 into ID_EX
    // ------------------------------------------------------------------------
    mem_read_data.write_value(0);  // Inst 1 was ADD (no memory read data)
    ex_alu_result.write_value(222);// Inst 2 ALU result
    id_rd.write_value(7);          // Inst 3 into ID_EX

    clock.write(logic::LogicState::LOW);
    id_ex.evaluate();
    ex_mem.evaluate();
    mem_wb.evaluate();

    clock.write(logic::LogicState::HIGH);
    id_ex.evaluate();
    ex_mem.evaluate();
    mem_wb.evaluate();

    // VERIFY ALL THREE CONCURRENTLY!
    assert(id_ex.rd().read_value() == 7);           // Inst 3 in ID_EX
    assert(ex_mem.rd().read_value() == 4);          // Inst 2 in EX_MEM
    assert(ex_mem.alu_result().read_value() == 222);
    assert(mem_wb.rd().read_value() == 1);          // Inst 1 in MEM_WB
    assert(mem_wb.alu_result().read_value() == 111);

    // ------------------------------------------------------------------------
    // Cycle 4: Inst 2 -> MEM_WB; Inst 3 -> EX_MEM
    // ------------------------------------------------------------------------
    ex_alu_result.write_value(333); // Inst 3 address/ALU result

    clock.write(logic::LogicState::LOW);
    id_ex.evaluate();
    ex_mem.evaluate();
    mem_wb.evaluate();

    clock.write(logic::LogicState::HIGH);
    id_ex.evaluate();
    ex_mem.evaluate();
    mem_wb.evaluate();

    assert(ex_mem.rd().read_value() == 7);          // Inst 3 in EX_MEM
    assert(ex_mem.alu_result().read_value() == 333);
    assert(mem_wb.rd().read_value() == 4);          // Inst 2 in MEM_WB
    assert(mem_wb.alu_result().read_value() == 222);

    std::cout << "  [PASS] Test 7 passed successfully.\n";
}


// Test 8: Test the control signals separately
void test_8_control_signals_separately()
{
    std::cout << "[Test 8] Running Control Signals Test...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    logic::Bus<32> pc;
    logic::Bus<32> rd1, rd2, imm;
    logic::Bus<5> rs, rt, rd;

    cpu::ID_EX<32, 32, 5> id_ex(pc, rd1, rd2, imm, rs, rt, rd, clock, reset, enable);
    cpu::EX_MEM<32, 5> ex_mem(rd1, rd2, rd, clock, reset, enable);
    cpu::MEM_WB<32, 5> mem_wb(rd1, rd2, rd, clock, reset, enable);

    // Send specified control signals:
    // RegDst   = 1
    // ALUSrc   = 0
    // ALUOp    = ADD
    // Branch   = 0
    // MemRead  = 1
    // MemWrite = 0
    // RegWrite = 1
    // MemToReg = 1
    cpu::PipelinedControlSignals ctrl_in{};
    ctrl_in.regDst = true;
    ctrl_in.aluSrc = false;
    ctrl_in.aluOp = cpu::ALUOperation::ADD;
    ctrl_in.branch = false;
    ctrl_in.memRead = true;
    ctrl_in.memWrite = false;
    ctrl_in.regWrite = true;
    ctrl_in.memToReg = true;

    id_ex.set_controls(ctrl_in);
    clock_component(clock, id_ex);

    // 1. Verify emerging from ID_EX
    assert(id_ex.reg_dst() == true);
    assert(id_ex.alu_src() == false);
    assert(id_ex.alu_op() == cpu::ALUOperation::ADD);
    assert(id_ex.branch() == false);
    assert(id_ex.mem_read() == true);
    assert(id_ex.mem_write() == false);
    assert(id_ex.reg_write() == true);
    assert(id_ex.mem_to_reg() == true);

    // 2. Forward ID_EX -> EX_MEM
    ex_mem.set_controls(id_ex.controls());
    clock_component(clock, ex_mem);

    assert(ex_mem.branch() == false);
    assert(ex_mem.mem_read() == true);
    assert(ex_mem.mem_write() == false);
    assert(ex_mem.reg_write() == true);
    assert(ex_mem.mem_to_reg() == true);

    // 3. Forward EX_MEM -> MEM_WB
    mem_wb.set_controls(ex_mem.controls());
    clock_component(clock, mem_wb);

    assert(mem_wb.reg_write() == true);
    assert(mem_wb.mem_to_reg() == true);

    // 4. Test alternate control vector
    cpu::PipelinedControlSignals alt_ctrl{};
    alt_ctrl.regDst = false;
    alt_ctrl.aluSrc = true;
    alt_ctrl.aluOp = cpu::ALUOperation::SUB;
    alt_ctrl.branch = true;
    alt_ctrl.memRead = false;
    alt_ctrl.memWrite = true;
    alt_ctrl.regWrite = false;
    alt_ctrl.memToReg = false;

    id_ex.set_controls(alt_ctrl);
    clock_component(clock, id_ex);

    assert(id_ex.reg_dst() == false);
    assert(id_ex.alu_src() == true);
    assert(id_ex.alu_op() == cpu::ALUOperation::SUB);
    assert(id_ex.branch() == true);
    assert(id_ex.mem_read() == false);
    assert(id_ex.mem_write() == true);
    assert(id_ex.reg_write() == false);
    assert(id_ex.mem_to_reg() == false);

    ex_mem.set_controls(id_ex.controls());
    clock_component(clock, ex_mem);

    assert(ex_mem.branch() == true);
    assert(ex_mem.mem_read() == false);
    assert(ex_mem.mem_write() == true);
    assert(ex_mem.reg_write() == false);
    assert(ex_mem.mem_to_reg() == false);

    mem_wb.set_controls(ex_mem.controls());
    clock_component(clock, mem_wb);

    assert(mem_wb.reg_write() == false);
    assert(mem_wb.mem_to_reg() == false);

    std::cout << "  [PASS] Test 8 passed successfully.\n";
}

// ============================================================================
// Test 9: Test representative instructions
// ============================================================================
void test_9_representative_instructions()
{
    std::cout << "[Test 9] Running Representative Instructions Test...\n";

    logic::Wire clock(logic::LogicState::LOW);
    logic::Wire reset(logic::LogicState::LOW);
    logic::Wire enable(logic::LogicState::HIGH);

    logic::Bus<32> id_pc;
    logic::Bus<32> id_rd1, id_rd2, id_imm;
    logic::Bus<5> id_rs, id_rt, id_rd;
    cpu::ID_EX<32, 32, 5> id_ex(id_pc, id_rd1, id_rd2, id_imm, id_rs, id_rt, id_rd, clock, reset, enable);

    logic::Bus<32> ex_alu_result;
    cpu::EX_MEM<32, 5> ex_mem(ex_alu_result, id_ex.read_data2(), id_ex.rd(), clock, reset, enable);

    logic::Bus<32> mem_read_data;
    cpu::MEM_WB<32, 5> mem_wb(mem_read_data, ex_mem.alu_result(), ex_mem.rd(), clock, reset, enable);

    // 1. R-type: ADD R1, R2, R3
    // Tests: ALUResult, Rd, RegWrite, MemToReg
    {
        // Decode / ID stage
        id_rs.write_value(2);
        id_rt.write_value(3);
        id_rd.write_value(1);
        cpu::PipelinedControlSignals add_ctrl{};
        add_ctrl.regDst = true;
        add_ctrl.aluSrc = false;
        add_ctrl.aluOp = cpu::ALUOperation::ADD;
        add_ctrl.regWrite = true;
        add_ctrl.memToReg = false;
        id_ex.set_controls(add_ctrl);

        clock_component(clock, id_ex);

        // EX stage: compute ALU result (simulate R2=15 + R3=25 = 40)
        ex_alu_result.write_value(40);
        ex_mem.set_controls(id_ex.controls());

        clock_component(clock, ex_mem);

        assert(ex_mem.alu_result().read_value() == 40);
        assert(ex_mem.rd().read_value() == 1);
        assert(ex_mem.reg_write() == true);
        assert(ex_mem.mem_to_reg() == false);

        // WB stage
        mem_read_data.write_value(0);
        mem_wb.set_controls(ex_mem.controls());

        clock_component(clock, mem_wb);

        assert(mem_wb.alu_result().read_value() == 40);
        assert(mem_wb.rd().read_value() == 1);
        assert(mem_wb.reg_write() == true);
        assert(mem_wb.mem_to_reg() == false);
    }

    // 2. Load: LW R1, 0(R2)
    // Tests: ALUResult, ReadData, Rd, MemRead, RegWrite, MemToReg
    {
        id_rs.write_value(2);
        id_rt.write_value(1); // destination register in I-type
        id_rd.write_value(1);
        id_imm.write_value(0);

        cpu::PipelinedControlSignals lw_ctrl{};
        lw_ctrl.regDst = false;
        lw_ctrl.aluSrc = true;
        lw_ctrl.aluOp = cpu::ALUOperation::ADD;
        lw_ctrl.memRead = true;
        lw_ctrl.memWrite = false;
        lw_ctrl.regWrite = true;
        lw_ctrl.memToReg = true;
        id_ex.set_controls(lw_ctrl);

        clock_component(clock, id_ex);

        // EX stage: address = base (0x1000) + offset (0) = 0x1000
        ex_alu_result.write_value(0x1000);
        ex_mem.set_controls(id_ex.controls());

        clock_component(clock, ex_mem);

        assert(ex_mem.alu_result().read_value() == 0x1000);
        assert(ex_mem.rd().read_value() == 1);
        assert(ex_mem.mem_read() == true);
        assert(ex_mem.reg_write() == true);
        assert(ex_mem.mem_to_reg() == true);

        // MEM stage: read memory data
        mem_read_data.write_value(0xDEADBEEF);
        mem_wb.set_controls(ex_mem.controls());

        clock_component(clock, mem_wb);

        assert(mem_wb.read_data().read_value() == 0xDEADBEEF);
        assert(mem_wb.alu_result().read_value() == 0x1000);
        assert(mem_wb.rd().read_value() == 1);
        assert(mem_wb.reg_write() == true);
        assert(mem_wb.mem_to_reg() == true);
    }

    // 3. Store: SW R1, 0(R2)
    // Tests: ALUResult, ReadData2, MemWrite
    {
        id_rs.write_value(2); // base
        id_rd2.write_value(0xCAFE); // data to write from R1
        id_imm.write_value(0);

        cpu::PipelinedControlSignals sw_ctrl{};
        sw_ctrl.aluSrc = true;
        sw_ctrl.aluOp = cpu::ALUOperation::ADD;
        sw_ctrl.memWrite = true;
        sw_ctrl.regWrite = false;
        id_ex.set_controls(sw_ctrl);

        clock_component(clock, id_ex);

        // EX stage: address = 0x2000
        ex_alu_result.write_value(0x2000);
        ex_mem.set_controls(id_ex.controls());

        clock_component(clock, ex_mem);

        assert(ex_mem.alu_result().read_value() == 0x2000);
        assert(ex_mem.read_data2().read_value() == 0xCAFE);
        assert(ex_mem.mem_write() == true);
        assert(ex_mem.reg_write() == false);
    }

    // 4. Branch: BEQ R1, R2, label
    // Tests: Branch, ReadData1/2, branch-related data
    {
        id_rd1.write_value(50);
        id_rd2.write_value(50);
        id_imm.write_value(8); // offset

        cpu::PipelinedControlSignals beq_ctrl{};
        beq_ctrl.branch = true;
        beq_ctrl.aluOp = cpu::ALUOperation::SUB;
        beq_ctrl.regWrite = false;
        id_ex.set_controls(beq_ctrl);

        clock_component(clock, id_ex);

        assert(id_ex.read_data1().read_value() == 50);
        assert(id_ex.read_data2().read_value() == 50);
        assert(id_ex.immediate().read_value() == 8);
        assert(id_ex.branch() == true);
        assert(id_ex.alu_op() == cpu::ALUOperation::SUB);

        // EX stage: forward branch signal
        ex_alu_result.write_value(0); // R1 - R2 = 0 (equal)
        ex_mem.set_controls(id_ex.controls());

        clock_component(clock, ex_mem);

        assert(ex_mem.branch() == true);
        assert(ex_mem.read_data2().read_value() == 50);
        assert(ex_mem.alu_result().read_value() == 0);
    }

    std::cout << "  [PASS] Test 9 passed successfully.\n";
}

int main()
{
    std::cout << "========================================================\n";
    std::cout << "=== Running Comprehensive Pipelined Registers Tests ====\n";
    std::cout << "========================================================\n";

    test_1_basic_passthrough_capture();
    test_2_reset();
    test_3_enable_stall();
    test_4_reset_priority();
    test_5_clock_edge();
    test_6_widths();
    test_7_back_to_back_instructions();
    test_8_control_signals_separately();
    test_9_representative_instructions();

    std::cout << "\n[SUCCESS] All 9 Pipelined Registers Tests Passed!\n";
    return 0;
}
