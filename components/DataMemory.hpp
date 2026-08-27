#pragma once

#include <cstddef>
#include <vector>
#include <logic/sequential/memory/Memory.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/clock.hpp>
#include <logic/simulator/Component.hpp>

namespace cpu{
    template<std::size_t AddressWidth =32,
              std::size_t DataWidth =32>
    class DataMemory:public logic::Component
    {
        public:
            DataMemory(
                logic::Clock& clock,
                logic::Wire& reset,
                logic::Wire& read_enable,
                logic::Wire& write_enable,
                logic::Bus<AddressWidth>& address,
                logic::Bus<DataWidth>& write_data,
                logic::Bus<DataWidth>& read_data
            ): 
               clock_(clock),
               reset_(reset),
               read_enable_(read_enable),
               write_enable_(write_enable),
               address_(address),
               write_data_(write_data),
               read_data_(read_data),
               memory_(
                   clock_,
                   reset_,
                   request_,
                   command_,
                   ready_,
                   address_,
                   write_data_,
                   read_data_
               )
            {}

            void evaluate() noexcept override{
                /*
                 Determining whether the CPU is requesting a memory operation
                   request can either be read OR write
                */
                const bool read = read_enable_.read() == logic::LogicState::HIGH;
                const bool write = write_enable_.read() == logic::LogicState::HIGH;

                request_.write((read || write) ? logic::LogicState::HIGH : logic::LogicState::LOW);

                /*
                 command: 
                   LOW = read
                   HIGH = write
                */
                command_.write(
                    write ? logic::LogicState::HIGH : logic::LogicState::LOW
                );

                /*
                 evaluating the underlying memory
                */
                memory_.evaluate();
            }

            void load_rom(const std::vector<std::size_t>& contents) noexcept{
                memory_.load_rom(contents);
            }

            [[nodiscard]]
            logic::Bus<DataWidth>& read_data() noexcept{
                return read_data_;
            }

            [[nodiscard]]
            const logic::Bus<DataWidth>& read_data() const noexcept{
                return read_data_;
            }

            [[nodiscard]]
            logic::Wire& ready() noexcept{
                return ready_;
            }

            [[nodiscard]]
            const logic::Wire& ready() const noexcept{
                return ready_;
            }

        private:

           //CPU interface
           logic::Clock& clock_;
           logic::Wire& reset_;
           logic::Wire& read_enable_;
           logic::Wire& write_enable_;

           logic::Bus<AddressWidth>& address_;
           logic::Bus<DataWidth>& write_data_;
           logic::Bus<DataWidth>& read_data_;

           //internal memory control signals
           logic::Wire request_;
           logic::Wire command_;
           logic::Wire ready_;

           //memory interface
           logic::Memory<AddressWidth,DataWidth>memory_;
    };
}