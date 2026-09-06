#pragma once

#include <cstddef>
#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include "../core/ControlSignals.hpp"

namespace cpu {

    template <std::size_t AddressWidth = 32, std::size_t DataWidth = 32, std::size_t RegAddrWidth = 5>
    class ID_EX : public logic::Component {
    public:
        ID_EX(
            logic::Bus<AddressWidth>& pcplus4,
            logic::Bus<DataWidth>& readData1,
            logic::Bus<DataWidth>& readData2,
            logic::Bus<DataWidth>& immediate,
            logic::Bus<RegAddrWidth>& rs,
            logic::Bus<RegAddrWidth>& rt,
            logic::Bus<RegAddrWidth>& rd,
            logic::Wire& clock,
            logic::Wire& reset,
            logic::Wire& enable,
            const PipelinedControlSignals& initial_controls = PipelinedControlSignals{}
        ) : pcplus4_(pcplus4),
            readData1_(readData1),
            readData2_(readData2),
            immediate_(immediate),
            rs_(rs),
            rt_(rt),
            rd_(rd),
            clock_(clock),
            reset_(reset),
            enable_(enable),

            // Data muxes & registers
            enable_mux_pc(pcplus4_out_, pcplus4_, enable_, enable_out_pc_),
            reset_mux_pc(enable_out_pc_, zero_pc_, reset_, pcplus4_reg_in_),
            pcplus4_reg(pcplus4_reg_in_, clock_, pcplus4_out_),

            enable_mux_rd1(read_data1_out_, readData1_, enable_, enable_out_rd1_),
            reset_mux_rd1(enable_out_rd1_, zero_rd1_, reset_, rd1_reg_in_),
            readData1_reg_(rd1_reg_in_, clock_, read_data1_out_),

            enable_mux_rd2(read_data2_out_, readData2_, enable_, enable_out_rd2_),
            reset_mux_rd2(enable_out_rd2_, zero_rd2_, reset_, rd2_reg_in_),
            readData2_reg_(rd2_reg_in_, clock_, read_data2_out_),

            enable_mux_imm(immediate_out_, immediate_, enable_, enable_out_imm_),
            reset_mux_imm(enable_out_imm_, zero_imm_, reset_, imm_reg_in_),
            immediate_reg_(imm_reg_in_, clock_, immediate_out_),

            enable_mux_rs(rs_out_, rs_, enable_, enable_out_rs_),
            reset_mux_rs(enable_out_rs_, zero_rs_, reset_, rs_reg_in_),
            rs_reg(rs_reg_in_, clock_, rs_out_),

            enable_mux_rt(rt_out_, rt_, enable_, enable_out_rt_),
            reset_mux_rt(enable_out_rt_, zero_rt_, reset_, rt_reg_in_),
            rt_reg(rt_reg_in_, clock_, rt_out_),

            enable_mux_rd(rd_out_, rd_, enable_, enable_out_rd_),
            reset_mux_rd(enable_out_rd_, zero_rd_, reset_, rd_reg_in_),
            rd_reg(rd_reg_in_, clock_, rd_out_),

            // Control muxes & registers
            enable_mux_reg_dst(reg_dst_out_, reg_dst_in_, enable_, enable_out_reg_dst_),
            reset_mux_reg_dst(enable_out_reg_dst_, zero_1_, reset_, reg_dst_reg_in_),
            reg_dst_reg_(reg_dst_reg_in_, clock_, reg_dst_out_),

            enable_mux_alu_src(alu_src_out_, alu_src_in_, enable_, enable_out_alu_src_),
            reset_mux_alu_src(enable_out_alu_src_, zero_1_, reset_, alu_src_reg_in_),
            alu_src_reg_(alu_src_reg_in_, clock_, alu_src_out_),

            enable_mux_alu_op(alu_op_out_, alu_op_in_, enable_, enable_out_alu_op_),
            reset_mux_alu_op(enable_out_alu_op_, zero_3_, reset_, alu_op_reg_in_),
            alu_op_reg_(alu_op_reg_in_, clock_, alu_op_out_),

            enable_mux_branch(branch_out_, branch_in_, enable_, enable_out_branch_),
            reset_mux_branch(enable_out_branch_, zero_1_, reset_, branch_reg_in_),
            branch_reg_(branch_reg_in_, clock_, branch_out_),

            enable_mux_mem_read(mem_read_out_, mem_read_in_, enable_, enable_out_mem_read_),
            reset_mux_mem_read(enable_out_mem_read_, zero_1_, reset_, mem_read_reg_in_),
            mem_read_reg_(mem_read_reg_in_, clock_, mem_read_out_),

            enable_mux_mem_write(mem_write_out_, mem_write_in_, enable_, enable_out_mem_write_),
            reset_mux_mem_write(enable_out_mem_write_, zero_1_, reset_, mem_write_reg_in_),
            mem_write_reg_(mem_write_reg_in_, clock_, mem_write_out_),

            enable_mux_reg_write(reg_write_out_, reg_write_in_, enable_, enable_out_reg_write_),
            reset_mux_reg_write(enable_out_reg_write_, zero_1_, reset_, reg_write_reg_in_),
            reg_write_reg_(reg_write_reg_in_, clock_, reg_write_out_),

            enable_mux_mem_to_reg(mem_to_reg_out_, mem_to_reg_in_, enable_, enable_out_mem_to_reg_),
            reset_mux_mem_to_reg(enable_out_mem_to_reg_, zero_1_, reset_, mem_to_reg_reg_in_),
            mem_to_reg_reg_(mem_to_reg_reg_in_, clock_, mem_to_reg_out_)
        {
            zero_pc_.write(logic::LogicState::LOW);
            zero_rd1_.write(logic::LogicState::LOW);
            zero_rd2_.write(logic::LogicState::LOW);
            zero_imm_.write(logic::LogicState::LOW);
            zero_rs_.write(logic::LogicState::LOW);
            zero_rt_.write(logic::LogicState::LOW);
            zero_rd_.write(logic::LogicState::LOW);
            zero_1_.write(logic::LogicState::LOW);
            zero_3_.write(logic::LogicState::LOW);

            set_controls(initial_controls);
        }

        void set_controls(const PipelinedControlSignals& ctrl) noexcept {
            reg_dst_in_.write_value(ctrl.regDst ? 1 : 0);
            alu_src_in_.write_value(ctrl.aluSrc ? 1 : 0);
            alu_op_in_.write_value(static_cast<std::uint32_t>(ctrl.aluOp));
            branch_in_.write_value(ctrl.branch ? 1 : 0);
            mem_read_in_.write_value(ctrl.memRead ? 1 : 0);
            mem_write_in_.write_value(ctrl.memWrite ? 1 : 0);
            reg_write_in_.write_value(ctrl.regWrite ? 1 : 0);
            mem_to_reg_in_.write_value(ctrl.memToReg ? 1 : 0);
        }

        void set_reg_dst(bool v) noexcept { reg_dst_in_.write_value(v ? 1 : 0); }
        void set_alu_src(bool v) noexcept { alu_src_in_.write_value(v ? 1 : 0); }
        void set_alu_op(ALUOperation op) noexcept { alu_op_in_.write_value(static_cast<std::uint32_t>(op)); }
        void set_branch(bool v) noexcept { branch_in_.write_value(v ? 1 : 0); }
        void set_mem_read(bool v) noexcept { mem_read_in_.write_value(v ? 1 : 0); }
        void set_mem_write(bool v) noexcept { mem_write_in_.write_value(v ? 1 : 0); }
        void set_reg_write(bool v) noexcept { reg_write_in_.write_value(v ? 1 : 0); }
        void set_mem_to_reg(bool v) noexcept { mem_to_reg_in_.write_value(v ? 1 : 0); }

        void evaluate() noexcept override {
            // Data path
            enable_mux_pc.evaluate();
            reset_mux_pc.evaluate();
            pcplus4_reg.evaluate();

            enable_mux_rd1.evaluate();
            reset_mux_rd1.evaluate();
            readData1_reg_.evaluate();

            enable_mux_rd2.evaluate();
            reset_mux_rd2.evaluate();
            readData2_reg_.evaluate();

            enable_mux_imm.evaluate();
            reset_mux_imm.evaluate();
            immediate_reg_.evaluate();

            enable_mux_rs.evaluate();
            reset_mux_rs.evaluate();
            rs_reg.evaluate();

            enable_mux_rt.evaluate();
            reset_mux_rt.evaluate();
            rt_reg.evaluate();

            enable_mux_rd.evaluate();
            reset_mux_rd.evaluate();
            rd_reg.evaluate();

            // Control path
            enable_mux_reg_dst.evaluate();
            reset_mux_reg_dst.evaluate();
            reg_dst_reg_.evaluate();

            enable_mux_alu_src.evaluate();
            reset_mux_alu_src.evaluate();
            alu_src_reg_.evaluate();

            enable_mux_alu_op.evaluate();
            reset_mux_alu_op.evaluate();
            alu_op_reg_.evaluate();

            enable_mux_branch.evaluate();
            reset_mux_branch.evaluate();
            branch_reg_.evaluate();

            enable_mux_mem_read.evaluate();
            reset_mux_mem_read.evaluate();
            mem_read_reg_.evaluate();

            enable_mux_mem_write.evaluate();
            reset_mux_mem_write.evaluate();
            mem_write_reg_.evaluate();

            enable_mux_reg_write.evaluate();
            reset_mux_reg_write.evaluate();
            reg_write_reg_.evaluate();

            enable_mux_mem_to_reg.evaluate();
            reset_mux_mem_to_reg.evaluate();
            mem_to_reg_reg_.evaluate();
        }

        // Data output accessors
        [[nodiscard]] logic::Bus<AddressWidth>& pcplus4() noexcept { return pcplus4_out_; }
        [[nodiscard]] const logic::Bus<AddressWidth>& pcplus4() const noexcept { return pcplus4_out_; }
        [[nodiscard]] const logic::Bus<AddressWidth>& read_pcplus4() const noexcept { return pcplus4_out_; }

        [[nodiscard]] logic::Bus<DataWidth>& read_data1() noexcept { return read_data1_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& read_data1() const noexcept { return read_data1_out_; }

        [[nodiscard]] logic::Bus<DataWidth>& read_data2() noexcept { return read_data2_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& read_data2() const noexcept { return read_data2_out_; }

        [[nodiscard]] logic::Bus<DataWidth>& immediate() noexcept { return immediate_out_; }
        [[nodiscard]] const logic::Bus<DataWidth>& immediate() const noexcept { return immediate_out_; }

        [[nodiscard]] logic::Bus<RegAddrWidth>& rs() noexcept { return rs_out_; }
        [[nodiscard]] const logic::Bus<RegAddrWidth>& rs() const noexcept { return rs_out_; }

        [[nodiscard]] logic::Bus<RegAddrWidth>& rt() noexcept { return rt_out_; }
        [[nodiscard]] const logic::Bus<RegAddrWidth>& rt() const noexcept { return rt_out_; }

        [[nodiscard]] logic::Bus<RegAddrWidth>& rd() noexcept { return rd_out_; }
        [[nodiscard]] const logic::Bus<RegAddrWidth>& rd() const noexcept { return rd_out_; }

        // Control output accessors
        [[nodiscard]] bool reg_dst() const noexcept { return reg_dst_out_.read_value() != 0; }
        [[nodiscard]] bool alu_src() const noexcept { return alu_src_out_.read_value() != 0; }
        [[nodiscard]] ALUOperation alu_op() const noexcept { return static_cast<ALUOperation>(alu_op_out_.read_value()); }
        [[nodiscard]] bool branch() const noexcept { return branch_out_.read_value() != 0; }
        [[nodiscard]] bool mem_read() const noexcept { return mem_read_out_.read_value() != 0; }
        [[nodiscard]] bool mem_write() const noexcept { return mem_write_out_.read_value() != 0; }
        [[nodiscard]] bool reg_write() const noexcept { return reg_write_out_.read_value() != 0; }
        [[nodiscard]] bool mem_to_reg() const noexcept { return mem_to_reg_out_.read_value() != 0; }

        [[nodiscard]]
        PipelinedControlSignals controls() const noexcept {
            return {
                reg_dst(),
                alu_src(),
                alu_op(),
                branch(),
                mem_read(),
                mem_write(),
                reg_write(),
                mem_to_reg()
            };
        }

    private:
        // Inputs
        logic::Bus<AddressWidth>& pcplus4_;
        logic::Bus<DataWidth>& readData1_;
        logic::Bus<DataWidth>& readData2_;
        logic::Bus<DataWidth>& immediate_;
        logic::Bus<RegAddrWidth>& rs_;
        logic::Bus<RegAddrWidth>& rt_;
        logic::Bus<RegAddrWidth>& rd_;

        // Signals
        logic::Wire& clock_;
        logic::Wire& reset_;
        logic::Wire& enable_;

        // Control inputs (registered internally)
        logic::Bus<1> reg_dst_in_;
        logic::Bus<1> alu_src_in_;
        logic::Bus<3> alu_op_in_;
        logic::Bus<1> branch_in_;
        logic::Bus<1> mem_read_in_;
        logic::Bus<1> mem_write_in_;
        logic::Bus<1> reg_write_in_;
        logic::Bus<1> mem_to_reg_in_;

        // Outputs
        logic::Bus<AddressWidth> pcplus4_out_;
        logic::Bus<DataWidth> read_data1_out_;
        logic::Bus<DataWidth> read_data2_out_;
        logic::Bus<DataWidth> immediate_out_;
        logic::Bus<RegAddrWidth> rs_out_;
        logic::Bus<RegAddrWidth> rt_out_;
        logic::Bus<RegAddrWidth> rd_out_;

        logic::Bus<1> reg_dst_out_;
        logic::Bus<1> alu_src_out_;
        logic::Bus<3> alu_op_out_;
        logic::Bus<1> branch_out_;
        logic::Bus<1> mem_read_out_;
        logic::Bus<1> mem_write_out_;
        logic::Bus<1> reg_write_out_;
        logic::Bus<1> mem_to_reg_out_;

        // Internal zero buses
        logic::Bus<AddressWidth> zero_pc_;
        logic::Bus<DataWidth> zero_rd1_;
        logic::Bus<DataWidth> zero_rd2_;
        logic::Bus<DataWidth> zero_imm_;
        logic::Bus<RegAddrWidth> zero_rs_;
        logic::Bus<RegAddrWidth> zero_rt_;
        logic::Bus<RegAddrWidth> zero_rd_;
        logic::Bus<1> zero_1_;
        logic::Bus<3> zero_3_;

        // Internal enable/reset intermediate buses
        logic::Bus<AddressWidth> enable_out_pc_;
        logic::Bus<DataWidth> enable_out_rd1_;
        logic::Bus<DataWidth> enable_out_rd2_;
        logic::Bus<DataWidth> enable_out_imm_;
        logic::Bus<RegAddrWidth> enable_out_rs_;
        logic::Bus<RegAddrWidth> enable_out_rt_;
        logic::Bus<RegAddrWidth> enable_out_rd_;

        logic::Bus<1> enable_out_reg_dst_;
        logic::Bus<1> enable_out_alu_src_;
        logic::Bus<3> enable_out_alu_op_;
        logic::Bus<1> enable_out_branch_;
        logic::Bus<1> enable_out_mem_read_;
        logic::Bus<1> enable_out_mem_write_;
        logic::Bus<1> enable_out_reg_write_;
        logic::Bus<1> enable_out_mem_to_reg_;

        logic::Bus<AddressWidth> pcplus4_reg_in_;
        logic::Bus<DataWidth> rd1_reg_in_;
        logic::Bus<DataWidth> rd2_reg_in_;
        logic::Bus<DataWidth> imm_reg_in_;
        logic::Bus<RegAddrWidth> rs_reg_in_;
        logic::Bus<RegAddrWidth> rt_reg_in_;
        logic::Bus<RegAddrWidth> rd_reg_in_;

        logic::Bus<1> reg_dst_reg_in_;
        logic::Bus<1> alu_src_reg_in_;
        logic::Bus<3> alu_op_reg_in_;
        logic::Bus<1> branch_reg_in_;
        logic::Bus<1> mem_read_reg_in_;
        logic::Bus<1> mem_write_reg_in_;
        logic::Bus<1> reg_write_reg_in_;
        logic::Bus<1> mem_to_reg_reg_in_;

        // Data Multiplexers
        logic::Mux<AddressWidth> enable_mux_pc;
        logic::Mux<AddressWidth> reset_mux_pc;

        logic::Mux<DataWidth> enable_mux_rd1;
        logic::Mux<DataWidth> reset_mux_rd1;

        logic::Mux<DataWidth> enable_mux_rd2;
        logic::Mux<DataWidth> reset_mux_rd2;

        logic::Mux<DataWidth> enable_mux_imm;
        logic::Mux<DataWidth> reset_mux_imm;

        logic::Mux<RegAddrWidth> enable_mux_rs;
        logic::Mux<RegAddrWidth> reset_mux_rs;

        logic::Mux<RegAddrWidth> enable_mux_rt;
        logic::Mux<RegAddrWidth> reset_mux_rt;

        logic::Mux<RegAddrWidth> enable_mux_rd;
        logic::Mux<RegAddrWidth> reset_mux_rd;

        // Control Multiplexers
        logic::Mux<1> enable_mux_reg_dst;
        logic::Mux<1> reset_mux_reg_dst;

        logic::Mux<1> enable_mux_alu_src;
        logic::Mux<1> reset_mux_alu_src;

        logic::Mux<3> enable_mux_alu_op;
        logic::Mux<3> reset_mux_alu_op;

        logic::Mux<1> enable_mux_branch;
        logic::Mux<1> reset_mux_branch;

        logic::Mux<1> enable_mux_mem_read;
        logic::Mux<1> reset_mux_mem_read;

        logic::Mux<1> enable_mux_mem_write;
        logic::Mux<1> reset_mux_mem_write;

        logic::Mux<1> enable_mux_reg_write;
        logic::Mux<1> reset_mux_reg_write;

        logic::Mux<1> enable_mux_mem_to_reg;
        logic::Mux<1> reset_mux_mem_to_reg;

        // Pipeline registers
        logic::Register<AddressWidth> pcplus4_reg;
        logic::Register<DataWidth> readData1_reg_;
        logic::Register<DataWidth> readData2_reg_;
        logic::Register<DataWidth> immediate_reg_;
        logic::Register<RegAddrWidth> rs_reg;
        logic::Register<RegAddrWidth> rt_reg;
        logic::Register<RegAddrWidth> rd_reg;

        logic::Register<1> reg_dst_reg_;
        logic::Register<1> alu_src_reg_;
        logic::Register<3> alu_op_reg_;
        logic::Register<1> branch_reg_;
        logic::Register<1> mem_read_reg_;
        logic::Register<1> mem_write_reg_;
        logic::Register<1> reg_write_reg_;
        logic::Register<1> mem_to_reg_reg_;
    };

} // namespace cpu