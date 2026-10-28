#pragma once

#include <cstddef>

#include <logic/sequential/registers/register.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/simulator/Component.hpp>
#include <logic/signals/clock.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>

namespace cpu {

template <std::size_t VectorWidth = 128>
class VectorRegister : public logic::Component {
public:
    VectorRegister(
        logic::Bus<VectorWidth>& data,
        logic::Wire& clock,
        logic::Wire& we,
        logic::Wire& reset
    ) : data_(data),
        clock_(clock),
        we_(we),
        reset_(reset),
        load_mux_(output_, data_, we_, load_output_),
        reset_mux_(load_output_, zero_bus_, reset_, reg_input_),
        reg_(reg_input_, clock_, output_)
    {
        zero_bus_.write(logic::LogicState::LOW);
    }

    void evaluate() noexcept override {
        load_mux_.evaluate();
        reset_mux_.evaluate();
        reg_.evaluate();
    }

    [[nodiscard]] logic::Bus<VectorWidth>& output() noexcept {
        return output_;
    }

    [[nodiscard]] const logic::Bus<VectorWidth>& output() const noexcept {
        return output_;
    }

    [[nodiscard]] logic::Bus<VectorWidth>& q() noexcept {
        return output_;
    }

    [[nodiscard]] const logic::Bus<VectorWidth>& q() const noexcept {
        return output_;
    }

    [[nodiscard]] const logic::Bus<VectorWidth>& read() const noexcept {
        return output_;
    }

private:
    // Inputs
    logic::Bus<VectorWidth>& data_;
    logic::Wire& clock_;
    logic::Wire& we_;
    logic::Wire& reset_;

    // Internal buses
    logic::Bus<VectorWidth> zero_bus_;
    logic::Bus<VectorWidth> load_output_;
    logic::Bus<VectorWidth> reg_input_;

    // Output bus Q
    logic::Bus<VectorWidth> output_;

    // Multiplexers
    logic::Mux<VectorWidth> load_mux_;
    logic::Mux<VectorWidth> reset_mux_;

    // Sequential storage register
    logic::Register<VectorWidth> reg_;
};

} // namespace cpu
