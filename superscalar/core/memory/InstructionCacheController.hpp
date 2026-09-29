#pragma once

#include <array>
#include <cstddef>

#include "CacheLine.hpp"

#include <logic/simulator/Component.hpp>
#include <logic/combinational/multiplexers/Mux.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>
#include <logic/sequential/registers/register.hpp>

namespace cpu
{

template <
    std::size_t CPUAddressWidth = 32,
    std::size_t InstructionWidth = 32,
    std::size_t NumLines = 16,
    std::size_t InstructionsPerLine = 4
>
class InstructionCacheController : public logic::Component
{
    static_assert(
        CPUAddressWidth > 0,
        "CPUAddressWidth must be greater than zero."
    );

    static_assert(
        InstructionWidth > 0,
        "InstructionWidth must be greater than zero."
    );

    static_assert(
        InstructionWidth % 8 == 0,
        "InstructionWidth must be a multiple of 8."
    );

    static_assert(
        NumLines > 0,
        "NumLines must be greater than zero."
    );

    static_assert(
        InstructionsPerLine > 0,
        "InstructionsPerLine must be greater than zero."
    );

    static_assert(
        (InstructionsPerLine & (InstructionsPerLine - 1)) == 0,
        "InstructionsPerLine must be a power of two."
    );

public:

    static constexpr std::size_t InstructionBytes =
        InstructionWidth / 8;

    static constexpr std::size_t LineBytes =
        InstructionBytes * InstructionsPerLine;

    static constexpr std::size_t StateWidth = 3;

    enum class State
    {
        IDLE = 0,
        REFILL_REQUEST = 1,
        REFILL_WAIT = 2,
        INSTALL = 3,
        COMPLETE = 4
    };

    InstructionCacheController(
        logic::Wire& clock,
        logic::Wire& reset,

        logic::Wire& miss,
        logic::Bus<CPUAddressWidth>& miss_address,

        logic::Wire& memory_request,
        logic::Bus<CPUAddressWidth>& memory_address,
        logic::Wire& memory_response,
        logic::Bus<InstructionWidth>& memory_instruction,

        logic::Wire& refill_done,

        InstructionCacheLine<CPUAddressWidth, InstructionWidth, NumLines, InstructionsPerLine>& cache
    )
        : clock_(clock),
          reset_(reset),

          miss_(miss),
          miss_address_(miss_address),

          memory_request_(memory_request),
          memory_address_(memory_address),
          memory_response_(memory_response),
          memory_instruction_(memory_instruction),

          refill_done_(refill_done),

          cache_(cache),
          state_register_(
              next_state_bus_,
              clock_,
              state_bus_
          ),
          miss_address_register_(
              next_miss_address_,
              clock_,
              miss_address_register_output_
          ),
          line_base_register_(
              next_line_base_,
              clock_,
              line_base_register_output_
          ),
          refill_word_register_(
              next_refill_word_,
              clock_,
              refill_word_output_
          )
    {
    }

    void evaluate() noexcept override
    {
        // 1. Evaluating the sequential registers (updates on clock rising edge)
        state_register_.evaluate();
        miss_address_register_.evaluate();
        line_base_register_.evaluate();
        refill_word_register_.evaluate();

        // 2. Decoding the current registered state
        const State current_state = get_state();
        const std::size_t current_line_base = bus_to_value(line_base_register_output_);
        const std::size_t current_refill_word = bus_to_value(refill_word_output_);

        state_ = current_state;
        line_base_address_ = current_line_base;
        refill_word_ = current_refill_word;

        // 3. Default combinational outputs
        memory_request_.write(logic::LogicState::LOW);
        refill_done_.write(logic::LogicState::LOW);

        // 4. Default next values to hold current register outputs
        copy_bus(state_bus_, next_state_bus_);
        copy_bus(miss_address_register_output_, next_miss_address_);
        copy_bus(line_base_register_output_, next_line_base_);
        copy_bus(refill_word_output_, next_refill_word_);

        // 5. Combinational next-state / output logic
        switch (current_state)
        {
            case State::IDLE:
                evaluate_idle();
                break;

            case State::REFILL_REQUEST:
                evaluate_refill_request();
                break;

            case State::REFILL_WAIT:
                evaluate_refill_wait();
                break;

            case State::INSTALL:
                evaluate_install();
                break;

            case State::COMPLETE:
                evaluate_complete();
                break;
        }

        // 6. Reset override
        if (reset_.read() == logic::LogicState::HIGH)
        {
            write_state(State::IDLE, next_state_bus_);
            clear_bus(next_miss_address_);
            clear_bus(next_line_base_);
            clear_bus(next_refill_word_);

            memory_request_.write(logic::LogicState::LOW);
            refill_done_.write(logic::LogicState::LOW);
        }

        // 7. Re-evaluate registers so master latches capture newly computed next_* buses when clock is LOW
        state_register_.evaluate();
        miss_address_register_.evaluate();
        line_base_register_.evaluate();
        refill_word_register_.evaluate();
    }

    [[nodiscard]]
    State state() const noexcept
    {
        return get_state();
    }

    [[nodiscard]]
    std::size_t line_base_address() const noexcept
    {
        return bus_to_value(line_base_register_output_);
    }

    [[nodiscard]]
    std::size_t refill_word() const noexcept
    {
        return bus_to_value(refill_word_output_);
    }

    [[nodiscard]]
    const std::array<std::size_t, InstructionsPerLine>& refill_buffer() const noexcept
    {
        return refill_buffer_;
    }

private:

    /*
        Clock / reset
    */
    logic::Wire& clock_;
    logic::Wire& reset_;

    /*
        Cache miss interface
    */
    logic::Wire& miss_;
    logic::Bus<CPUAddressWidth>& miss_address_;

    /*
        Memory request / response interface
    */
    logic::Wire& memory_request_;
    logic::Bus<CPUAddressWidth>& memory_address_;
    logic::Wire& memory_response_;
    logic::Bus<InstructionWidth>& memory_instruction_;

    /*
        Refill completion signal
    */
    logic::Wire& refill_done_;

    /*
        Cache being controlled
    */
    InstructionCacheLine<CPUAddressWidth, InstructionWidth, NumLines, InstructionsPerLine>& cache_;

    /*
        State register
    */
    logic::Bus<StateWidth> state_bus_;
    logic::Bus<StateWidth> next_state_bus_;
    logic::Register<StateWidth> state_register_;

    /*
        Miss-address register
    */
    logic::Bus<CPUAddressWidth> next_miss_address_;
    logic::Bus<CPUAddressWidth> miss_address_register_output_;
    logic::Register<CPUAddressWidth> miss_address_register_;

    /*
        Cache-line base address register
    */
    logic::Bus<CPUAddressWidth> next_line_base_;
    logic::Bus<CPUAddressWidth> line_base_register_output_;
    logic::Register<CPUAddressWidth> line_base_register_;

    /*
        Refill-word counter register (8 bits wide to comfortably avoid overflow)
    */
    static constexpr std::size_t RefillWordBits = 8;
    logic::Bus<RefillWordBits> next_refill_word_;
    logic::Bus<RefillWordBits> refill_word_output_;
    logic::Register<RefillWordBits> refill_word_register_;

    /*
        Refill buffer
    */
    std::array<std::size_t, InstructionsPerLine> refill_buffer_{};

    /*
        Shadow state variables for fast const access
    */
    State state_ = State::IDLE;
    std::size_t refill_word_ = 0;
    std::size_t line_base_address_ = 0;

    /*
        Converting a Logic Bus into a normal integer
    */
    template <std::size_t BusWidth>
    [[nodiscard]]
    static std::size_t bus_to_value(
        const logic::Bus<BusWidth>& bus
    ) noexcept
    {
        std::size_t value = 0;
        for (std::size_t i = 0; i < BusWidth; ++i)
        {
            if (bus[i].read() == logic::LogicState::HIGH)
            {
                value |= (1ULL << i);
            }
        }
        return value;
    }

    /*
        Converting an integer into a Bus
    */
    template <std::size_t Width>
    static void value_to_bus(
        std::size_t value,
        logic::Bus<Width>& bus
    ) noexcept
    {
        for (std::size_t i = 0; i < Width; ++i)
        {
            bus[i].write(
                (value & (1ULL << i))
                    ? logic::LogicState::HIGH
                    : logic::LogicState::LOW
            );
        }
    }

    /*
        Copying one bus to another
    */
    template <std::size_t Width>
    static void copy_bus(
        const logic::Bus<Width>& source,
        logic::Bus<Width>& destination
    ) noexcept
    {
        for (std::size_t i = 0; i < Width; ++i)
        {
            destination[i].write(source[i].read());
        }
    }

    /*
        Clearing a bus
    */
    template <std::size_t Width>
    static void clear_bus(logic::Bus<Width>& bus) noexcept
    {
        for (std::size_t i = 0; i < Width; ++i)
        {
            bus[i].write(logic::LogicState::LOW);
        }
    }

    /*
        Writing an FSM state into the state bus
    */
    static void write_state(State state, logic::Bus<StateWidth>& bus) noexcept
    {
        value_to_bus(static_cast<std::size_t>(state), bus);
    }

    /*
        Reading the FSM state from the state register
    */
    [[nodiscard]]
    State get_state() const noexcept
    {
        const std::size_t value = bus_to_value(state_bus_);
        return static_cast<State>(value);
    }

    /*
        IDLE: Waiting for the cache to report a miss
    */
    void evaluate_idle() noexcept
    {
        if (miss_.read() != logic::LogicState::HIGH)
        {
            return;
        }

        copy_bus(miss_address_, next_miss_address_);
        const std::size_t miss_address = bus_to_value(miss_address_);
        const std::size_t line_base = miss_address & ~(LineBytes - 1);

        value_to_bus(line_base, next_line_base_);
        value_to_bus(0, next_refill_word_);
        write_state(State::REFILL_REQUEST, next_state_bus_);
    }

    /*
        REFILL_REQUEST: Driving the next memory request onto the interface
    */
    void evaluate_refill_request() noexcept
    {
        const std::size_t line_base = bus_to_value(line_base_register_output_);
        const std::size_t word = bus_to_value(refill_word_output_);
        const std::size_t address = line_base + word * InstructionBytes;

        value_to_bus(address, memory_address_);
        memory_request_.write(logic::LogicState::HIGH);

        write_state(State::REFILL_WAIT, next_state_bus_);
    }

    /*
        REFILL_WAIT: Waiting until memory responds with the requested instruction
    */
    void evaluate_refill_wait() noexcept
    {
        const std::size_t line_base = bus_to_value(line_base_register_output_);
        const std::size_t word = bus_to_value(refill_word_output_);
        const std::size_t address = line_base + word * InstructionBytes;

        value_to_bus(address, memory_address_);
        memory_request_.write(logic::LogicState::HIGH);

        if (memory_response_.read() != logic::LogicState::HIGH)
        {
            return;
        }

        if (word < InstructionsPerLine)
        {
            refill_buffer_[word] = bus_to_value(memory_instruction_);
        }

        if (word + 1 >= InstructionsPerLine)
        {
            value_to_bus(word + 1, next_refill_word_);
            write_state(State::INSTALL, next_state_bus_);
        }
        else
        {
            value_to_bus(word + 1, next_refill_word_);
            write_state(State::REFILL_REQUEST, next_state_bus_);
        }
    }

    /*
        INSTALL: Writing the assembled cache line into the cache
    */
    void evaluate_install() noexcept
    {
        const std::size_t line_base = bus_to_value(line_base_register_output_);
        const std::size_t line_index =
            InstructionCacheLine<
                CPUAddressWidth,
                InstructionWidth,
                NumLines,
                InstructionsPerLine
            >::get_line_index(line_base);

        const std::size_t tag =
            InstructionCacheLine<
                CPUAddressWidth,
                InstructionWidth,
                NumLines,
                InstructionsPerLine
            >::get_tag(line_base);

        cache_.refill(
            line_index,
            tag,
            refill_buffer_
        );

        write_state(State::COMPLETE, next_state_bus_);
    }

    /*
        COMPLETE: Pulsing refill_done to indicate line refill completion
    */
    void evaluate_complete() noexcept
    {
        refill_done_.write(logic::LogicState::HIGH);
        write_state(State::IDLE, next_state_bus_);
    }
};

} // namespace cpu
