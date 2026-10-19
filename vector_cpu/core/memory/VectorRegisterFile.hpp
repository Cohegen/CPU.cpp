#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/combinational/decoders/Decoder.hpp>
#include <logic/combinational/multiplexers/MuxTree.hpp>
#include <logic/gates/AND.hpp>
#include <logic/simulator/Component.hpp>

#include "../registers/VectorRegister.hpp"

namespace cpu
{

namespace detail
{
    constexpr std::size_t clog2(std::size_t val)
    {
        std::size_t bits = 0;
        while ((static_cast<std::size_t>(1) << bits) < val)
        {
            ++bits;
        }
        return bits;
    }
} // namespace detail


template <
    std::size_t NumRegisters = 32,
    std::size_t DataWidth = 128,
    std::size_t AddressWidth = detail::clog2(NumRegisters)
>
class VectorRegisterFile : public logic::Component
{
    static_assert(NumRegisters >= 2, "VectorRegisterFile requires at least 2 registers.");
    static_assert((NumRegisters & (NumRegisters - 1)) == 0, "NumRegisters must be a power of 2.");
    static_assert(AddressWidth >= detail::clog2(NumRegisters), "AddressWidth is insufficient for NumRegisters.");

public:
   
    VectorRegisterFile(
        logic::Bus<AddressWidth>& read_addr1,
        logic::Bus<AddressWidth>& read_addr2,
        logic::Bus<AddressWidth>& write_addr,
        logic::Bus<DataWidth>& write_data,
        logic::Wire& write_enable,
        logic::Wire& reset,
        logic::Wire& clock
    ) : read_addr1_(read_addr1),
        read_addr2_(read_addr2),
        write_addr_(write_addr),
        write_data_(write_data),
        write_enable_(write_enable),
        reset_(reset),
        clock_(clock),
        write_decoder_(write_addr_, decoded_lines_)
    {
        init_internal_hardware();
    }

    /// Overloaded constructor matching alternate argument order
    VectorRegisterFile(
        logic::Bus<AddressWidth>& read_addr1,
        logic::Bus<AddressWidth>& read_addr2,
        logic::Wire& write_enable,
        logic::Wire& clock,
        logic::Wire& reset,
        logic::Bus<AddressWidth>& write_addr,
        logic::Bus<DataWidth>& write_data
    ) : read_addr1_(read_addr1),
        read_addr2_(read_addr2),
        write_addr_(write_addr),
        write_data_(write_data),
        write_enable_(write_enable),
        reset_(reset),
        clock_(clock),
        write_decoder_(write_addr_, decoded_lines_)
    {
        init_internal_hardware();
    }

    void evaluate() noexcept override
    {
        // 1. Evaluate write-address decoding
        write_decoder_.evaluate();
        for (auto& and_gate : we_and_gates_)
        {
            and_gate->evaluate();
        }

        // 2. Evaluate all vector registers
        for (auto& reg : registers_)
        {
            reg->evaluate();
        }

        // 3. Evaluate read multiplexers
        read_mux1_->evaluate();
        read_mux2_->evaluate();
    }

    // Access read-port outputs
    [[nodiscard]] logic::Bus<DataWidth>& read_data1() noexcept
    {
        return read_data1_;
    }

    [[nodiscard]] const logic::Bus<DataWidth>& read_data1() const noexcept
    {
        return read_data1_;
    }

    [[nodiscard]] logic::Bus<DataWidth>& read_data2() noexcept
    {
        return read_data2_;
    }

    [[nodiscard]] const logic::Bus<DataWidth>& read_data2() const noexcept
    {
        return read_data2_;
    }

    // Direct access to individual registers for verification/inspection
    [[nodiscard]] VectorRegister<DataWidth>& get_register(std::size_t index)
    {
        return *registers_.at(index);
    }

    [[nodiscard]] const VectorRegister<DataWidth>& get_register(std::size_t index) const
    {
        return *registers_.at(index);
    }

    [[nodiscard]] VectorRegister<DataWidth>& operator[](std::size_t index)
    {
        return *registers_.at(index);
    }

    [[nodiscard]] const VectorRegister<DataWidth>& operator[](std::size_t index) const
    {
        return *registers_.at(index);
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return NumRegisters;
    }

private:
    void init_internal_hardware()
    {
        // 1. Initialize AND gates for register write enables:
        //    WE_i = GlobalWE AND (WriteAddress == i)
        we_and_gates_.reserve(NumRegisters);
        for (std::size_t i = 0; i < NumRegisters; ++i)
        {
            we_and_gates_.push_back(
                std::make_unique<logic::ANDGate>(
                    decoded_lines_[i],
                    write_enable_,
                    register_write_enables_[i]
                )
            );
        }

        // 2. Initialize NumRegisters VectorRegister instances with stable references
        registers_.reserve(NumRegisters);
        for (std::size_t i = 0; i < NumRegisters; ++i)
        {
            registers_.push_back(
                std::make_unique<VectorRegister<DataWidth>>(
                    write_data_,
                    clock_,
                    register_write_enables_[i],
                    reset_
                )
            );
        }

        // 3. Connect all register output buses to both read multiplexer trees
        std::vector<logic::Bus<DataWidth>*> reg_outputs;
        reg_outputs.reserve(NumRegisters);
        for (std::size_t i = 0; i < NumRegisters; ++i)
        {
            reg_outputs.push_back(&registers_[i]->output());
        }

        read_mux1_ = std::make_unique<logic::MuxTree<NumRegisters, DataWidth, AddressWidth>>(
            reg_outputs,
            read_addr1_,
            read_data1_
        );

        read_mux2_ = std::make_unique<logic::MuxTree<NumRegisters, DataWidth, AddressWidth>>(
            reg_outputs,
            read_addr2_,
            read_data2_
        );
    }

    // External input signals
    logic::Bus<AddressWidth>& read_addr1_;
    logic::Bus<AddressWidth>& read_addr2_;
    logic::Bus<AddressWidth>& write_addr_;
    logic::Bus<DataWidth>& write_data_;
    logic::Wire& write_enable_;
    logic::Wire& reset_;
    logic::Wire& clock_;

    // Internal register write-enable signals & decoded lines
    logic::Bus<NumRegisters> decoded_lines_;
    logic::Bus<NumRegisters> register_write_enables_;

    // Write decoder
    logic::Decoder<AddressWidth, NumRegisters> write_decoder_;
    std::vector<std::unique_ptr<logic::ANDGate>> we_and_gates_;

    // Storage: NumRegisters VectorRegister instances
    std::vector<std::unique_ptr<VectorRegister<DataWidth>>> registers_;

    // Read multiplexer trees
    std::unique_ptr<logic::MuxTree<NumRegisters, DataWidth, AddressWidth>> read_mux1_;
    std::unique_ptr<logic::MuxTree<NumRegisters, DataWidth, AddressWidth>> read_mux2_;

    // Read output buses
    logic::Bus<DataWidth> read_data1_;
    logic::Bus<DataWidth> read_data2_;
};

} // namespace cpu
