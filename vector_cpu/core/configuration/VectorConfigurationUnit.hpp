#pragma once

#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/simulator/Component.hpp>

#include "VLMAXUnit.hpp"
#include "MinimumUnit.hpp"

namespace cpu {

class VectorConfigurationUnit : public logic::Component {
public:
    VectorConfigurationUnit(
        logic::Bus<8>& avl,
        logic::Bus<2>& sew,
        logic::Wire& clock,
        logic::Wire& configure,
        logic::Wire& reset
    ) : avl_(avl),
        sew_(sew),
        clock_(clock),
        configure_(configure),
        reset_(reset),
        vlmax_unit_(sew_, vlmax_bus_, vlmax_valid_wire_),
        minimum_unit_(avl_, vlmax_bus_, vl_next_),
        load_mux_vl_(vl_out_, vl_next_, write_enable_, vl_load_out_),
        reset_mux_vl_(vl_load_out_, zero_bus_8_, reset_, vl_reg_in_),
        vl_reg_(vl_reg_in_, clock_, vl_out_),
        load_mux_vtype_(vtype_out_, sew_, write_enable_, vtype_load_out_),
        reset_mux_vtype_(vtype_load_out_, zero_bus_2_, reset_, vtype_reg_in_),
        vtype_reg_(vtype_reg_in_, clock_, vtype_out_)
    {
        zero_bus_8_.write(logic::LogicState::LOW);
        zero_bus_2_.write(logic::LogicState::LOW);
        vl_out_.write(logic::LogicState::LOW);
        vtype_out_.write(logic::LogicState::LOW);
        vl_load_out_.write(logic::LogicState::LOW);
        vl_reg_in_.write(logic::LogicState::LOW);
        vtype_load_out_.write(logic::LogicState::LOW);
        vtype_reg_in_.write(logic::LogicState::LOW);
    }

    void evaluate_combinational() noexcept {
        // 1. Evaluate VLMAX combinational unit
        vlmax_unit_.evaluate();

        // 2. Evaluate validity:
        // Check if sew is valid (from VLMAXUnit)
        const logic::LogicState sew_valid = vlmax_valid_wire_.read();

        // Check if all bits of AVL are known binary values (LOW or HIGH)
        bool avl_all_known = true;
        for (std::size_t i = 0; i < 8; ++i) {
            if (!logic::is_binary(avl_[i].read())) {
                avl_all_known = false;
                break;
            }
        }

        // Validity policy:
        // If SEW is invalid (LOW), validity is LOW.
        // If SEW validity is UNKNOWN, validity is UNKNOWN.
        // If SEW is valid (HIGH):
        //    if AVL is completely known, validity is HIGH.
        //    else validity is UNKNOWN.
        logic::LogicState current_valid = logic::LogicState::LOW;
        if (sew_valid == logic::LogicState::LOW) {
            current_valid = logic::LogicState::LOW;
        } else if (sew_valid == logic::LogicState::UNKNOWN) {
            current_valid = logic::LogicState::UNKNOWN;
        } else {
            current_valid = avl_all_known ? logic::LogicState::HIGH : logic::LogicState::UNKNOWN;
        }
        valid_.write(current_valid);

        // 3. Evaluate minimum unit min(avl, vlmax)
        minimum_unit_.evaluate();

        // 4. Determine write enable
        // Write occurs only if configure is HIGH AND valid is HIGH.
        // If configure is UNKNOWN or valid is UNKNOWN or LOW, do NOT commit write.
        const bool should_write = (configure_.read() == logic::LogicState::HIGH &&
                                   current_valid == logic::LogicState::HIGH);
        write_enable_.write(should_write ? logic::LogicState::HIGH : logic::LogicState::LOW);

        // 5. Evaluate feedback and reset multiplexers
        load_mux_vl_.evaluate();
        reset_mux_vl_.evaluate();

        load_mux_vtype_.evaluate();
        reset_mux_vtype_.evaluate();
    }

    void evaluate_registers() noexcept {
        vl_reg_.evaluate();
        vtype_reg_.evaluate();
    }

    void evaluate() noexcept override {
        evaluate_combinational();
        evaluate_registers();
    }

    // Getters for architectural state & outputs
    [[nodiscard]] logic::Bus<8>& vl() noexcept { return vl_out_; }
    [[nodiscard]] const logic::Bus<8>& vl() const noexcept { return vl_out_; }

    [[nodiscard]] logic::Bus<2>& vtype() noexcept { return vtype_out_; }
    [[nodiscard]] const logic::Bus<2>& vtype() const noexcept { return vtype_out_; }

    [[nodiscard]] logic::Bus<8>& vlmax() noexcept { return vlmax_bus_; }
    [[nodiscard]] const logic::Bus<8>& vlmax() const noexcept { return vlmax_bus_; }

    [[nodiscard]] logic::Bus<8>& next_vl() noexcept { return vl_next_; }
    [[nodiscard]] const logic::Bus<8>& next_vl() const noexcept { return vl_next_; }

    [[nodiscard]] logic::Wire& valid() noexcept { return valid_; }
    [[nodiscard]] const logic::Wire& valid() const noexcept { return valid_; }

    [[nodiscard]] logic::Wire& write_enable() noexcept { return write_enable_; }
    [[nodiscard]] const logic::Wire& write_enable() const noexcept { return write_enable_; }

private:
    // External inputs & control references
    logic::Bus<8>& avl_;
    logic::Bus<2>& sew_;
    logic::Wire& clock_;
    logic::Wire& configure_;
    logic::Wire& reset_;

    // Internal and output signals
    logic::Bus<8> vlmax_bus_;
    logic::Wire vlmax_valid_wire_{logic::LogicState::LOW};
    logic::Wire valid_{logic::LogicState::LOW};
    logic::Wire write_enable_{logic::LogicState::LOW};

    logic::Bus<8> vl_next_;
    logic::Bus<8> vl_out_;
    logic::Bus<8> vl_load_out_;
    logic::Bus<8> vl_reg_in_;
    logic::Bus<8> zero_bus_8_;

    logic::Bus<2> vtype_out_;
    logic::Bus<2> vtype_load_out_;
    logic::Bus<2> vtype_reg_in_;
    logic::Bus<2> zero_bus_2_;

    // Internal hardware components
    VLMAXUnit vlmax_unit_;
    MinimumUnit<8> minimum_unit_;

    logic::Mux<8> load_mux_vl_;
    logic::Mux<8> reset_mux_vl_;
    logic::Register<8> vl_reg_;

    logic::Mux<2> load_mux_vtype_;
    logic::Mux<2> reset_mux_vtype_;
    logic::Register<2> vtype_reg_;
};

} // namespace cpu
