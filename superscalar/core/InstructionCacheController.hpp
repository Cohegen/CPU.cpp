#pragma once

#include <array>
#include <cstddef>

#include "CacheLine.hpp"

#include <logic/simulator/Component.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
#include <logic/signals/wire.hpp>

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


    enum class State
    {
        IDLE,
        REFILL_REQUEST,
        REFILL_WAIT,
        INSTALL,
        COMPLETE
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

        InstructionCacheLine<CPUAddressWidth,InstructionWidth,NumLines,InstructionsPerLine>& cache
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

          cache_(cache)
    {
    }


    void evaluate() noexcept override
    {
        /*
            refill_done is a pulse
            Unless we are in COMPLETE, it remains LOW
        */

        refill_done_.write(logic::LogicState::LOW);


        /*
            resetting controller state
        */

        if (reset_.read() == logic::LogicState::HIGH)
        {
            state_ = State::IDLE;

            refill_word_ = 0;
            miss_address_value_ = 0;
            line_base_address_ = 0;

            memory_request_.write(
                logic::LogicState::LOW
            );

            return;
        }


        switch (state_)
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
    }


    [[nodiscard]]
    State state() const noexcept
    {
        return state_;
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

    InstructionCacheLine<CPUAddressWidth,InstructionWidth,NumLines,InstructionsPerLine>& cache_;


    /*
        Controller state
    */

    State state_ = State::IDLE;


    /*
        Refill bookkeeping
    */

    std::size_t refill_word_ = 0;

    std::size_t miss_address_value_ = 0;

    std::size_t line_base_address_ = 0;


    /*
        Temporary line-fill buffer
    */

    std::array<std::size_t,InstructionsPerLine> refill_buffer_{};


    /*
        converting a Logic Bus into a normal integer
    */

    [[nodiscard]]
    template <std::size_t BusWidth>
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
        Converting an integer into a CPU address bus
    */

    static void value_to_bus(
        std::size_t value,
        logic::Bus<CPUAddressWidth>& bus
    ) noexcept
    {
        for (std::size_t i = 0; i < CPUAddressWidth; ++i)
        {
            bus[i].write(
                (value & (1ULL << i))
                    ? logic::LogicState::HIGH
                    : logic::LogicState::LOW
            );
        }
    }


    /*
        IDLE

        waiting for the cache to report a miss.
    */

    void evaluate_idle() noexcept
    {
        memory_request_.write(logic::LogicState::LOW);


        if (miss_.read() != logic::LogicState::HIGH)
        {
            return;
        }


        /*
            Capturing the address that missed
        */

        miss_address_value_ =
            bus_to_value(miss_address_);


        /*
            Aligning the address to the beginning
            of the cache line
        */

        line_base_address_ =miss_address_value_& ~(LineBytes - 1);


        /*
            beginning fetching the first word
        */

        refill_word_ = 0;


        state_ = State::REFILL_REQUEST;
    }


    /*
        REFILL_REQUEST

        putting the next memory request onto the interface
    */

    void evaluate_refill_request() noexcept
    {
        /*
            Calculating the address of the next
            instruction in the cache line
        */

        const std::size_t address =
            line_base_address_
            + refill_word_ * InstructionBytes;


        /*
            driving the memory interface.
        */

        value_to_bus(address, memory_address_);


        memory_request_.write(logic::LogicState::HIGH);


        /*
            issuing the request
        */

        state_ = State::REFILL_WAIT;
    }


    /*
        REFILL_WAIT

        Waiting until memory tells us that the
        requested instruction is available
    */

    void evaluate_refill_wait() noexcept
    {
        /*
            Request is only asserted while issuing
            the request
        */

        memory_request_.write(
            logic::LogicState::LOW
        );


        /*
            Memory hasn't responded yet.
        */

        if (memory_response_.read() != logic::LogicState::HIGH)
        {
            return;
        }


        /*
            Memory response arrived
            Capturing the instruction
        */

        refill_buffer_[refill_word_] =
            bus_to_value(memory_instruction_);


        /*
            moving to the next instruction in the cache line
        */

        ++refill_word_;


        if (refill_word_ == InstructionsPerLine)
        {
            /*
                entire cache line has been fetched
            */

            state_ = State::INSTALL;
        }
        else
        {
            /*
                More words remain
            */

            state_ = State::REFILL_REQUEST;
        }
    }


    /*
        INSTALL

        putting the completed line into the cache
    */

    void evaluate_install() noexcept
    {
        const std::size_t line_index = InstructionCacheLine<CPUAddressWidth,InstructionWidth,
                NumLines,
                InstructionsPerLine
            >::get_line_index(
                line_base_address_
            );


        const std::size_t tag =
            InstructionCacheLine<
                CPUAddressWidth,
                InstructionWidth,
                NumLines,
                InstructionsPerLine
            >::get_tag(
                line_base_address_
            );


        cache_.refill(
            line_index,
            tag,
            refill_buffer_
        );


        state_ = State::COMPLETE;
    }


    /*
        COMPLETE

        telling the cache/fetch side that the refill
        has completed
    */

    void evaluate_complete() noexcept
    {
        refill_done_.write(
            logic::LogicState::HIGH
        );


        state_ = State::IDLE;
    }
};

} // namespace cpu
