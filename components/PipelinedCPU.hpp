#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <logic/signals/clock.hpp>
#include <logic/signals/wire.hpp>
#include "../pipelined_cpu/datapath/PipelinedDatapath.hpp"

namespace cpu {

template<
    std::size_t AddressWidth = 32,
    std::size_t DataWidth = 32,
    std::size_t InstructionWidth = 32,
    std::size_t RegisterAddressWidth = 4,
    std::size_t InstructionMemoryAddressWidth = 8,
    std::size_t DataMemoryAddressWidth = 8
>
class PipelinedCPU {
public:
    PipelinedCPU() : datapath_(clock_, reset_) {}

    void reset() noexcept {
        datapath_.reset();
    }

    /// Advances simulation by one clock cycle across all 5 pipeline stages.
    void step() noexcept {
        datapath_.step();
    }

    /// Runs cycles autonomously until the HALT instruction reaches Writeback (or max_cycles reached).
    void run(std::size_t max_cycles = 100000) noexcept {
        datapath_.run(max_cycles);
    }

    [[nodiscard]]
    bool halted() const noexcept {
        return datapath_.halted();
    }

    void load_program(const std::vector<std::size_t>& instructions) noexcept {
        datapath_.load_instructions(instructions);
    }

    void load_program(const std::vector<std::uint32_t>& instructions) noexcept {
        datapath_.load_instructions(instructions);
    }

    [[nodiscard]]
    const PipelinedCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        InstructionMemoryAddressWidth,
        DataMemoryAddressWidth
    >& datapath() const noexcept {
        return datapath_;
    }

    [[nodiscard]]
    PipelinedCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        InstructionMemoryAddressWidth,
        DataMemoryAddressWidth
    >& datapath() noexcept {
        return datapath_;
    }

    [[nodiscard]]
    std::uint32_t read_register(Register reg) noexcept {
        return datapath_.read_register_for_test(reg);
    }

    void write_register(Register reg, std::uint32_t value) noexcept {
        datapath_.write_register_for_test(reg, value);
    }

    [[nodiscard]]
    std::uint32_t read_memory(std::size_t address) noexcept {
        return datapath_.read_memory_for_test(address);
    }

    void write_memory(std::size_t address, std::uint32_t value) noexcept {
        datapath_.write_memory_for_test(address, value);
    }

    [[nodiscard]]
    std::uint32_t pc() const noexcept {
        return static_cast<std::uint32_t>(datapath_.pc().read_value());
    }

    [[nodiscard]]
    std::uint64_t cycles() const noexcept {
        return clock_.cycle() / 2;
    }

private:
    logic::Clock clock_;
    logic::Wire reset_;

    PipelinedCycleDatapath<
        AddressWidth,
        DataWidth,
        InstructionWidth,
        RegisterAddressWidth,
        InstructionMemoryAddressWidth,
        DataMemoryAddressWidth
    > datapath_;
};

} // namespace cpu
