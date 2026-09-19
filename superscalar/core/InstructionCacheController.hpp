#pragma once
#include <logic/simulator/component.hpp>
#include <logic/signals/wire.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/logicState.hpp>
namespace cpu{
    

    template<
    std::size_t CPUAddressWidth=32,
    std::size_t InstructionWidth=32,
    std::size_t InstructionsPerLine=4 
    >
    class InstructionCacheController : public logic::Component{
        static_assert(CPUAddressWidth >0,"CPUAddressWidth must be greater than zero");
        static_assert(InstructionWidth>0,"InstructionWidth must be greater than zero");
        static_assert(InstructionWidth % 8==0,"InstructionWidth must be a multiple of 8");
        static_assert(InstructionsPerLine >0,"InstructionsPerLine must be greater than zero.");
        static_assert(
            (InstructionsPerLine & (InstructionsPerLine - 1)) == 0,
            "InstructionsPerLine must be a power of two."
        );
        public:
        static constexpr std::size_t InstructionBytes = InstructionWidth /8;
        static constexpr std::size_t LineBytes = InstructionBytes * InstructionsPerLine;
        enum class State
          {
            IDLE,
            REFILL,
            INSTALL,
            COMPLETE
         };

        InstructionCacheController(
            logic::Wire& clock,
            logic::Wire& reset,
            logic::Wire& miss,
            logic::Bus<CPUAddressWidth>& miss_address,
            logic::Bus<CPUAddressWidth>& memory_address,
            logic::Bus<InstructionWidth>& memory_instruction,
            logic::Bus<InstructionWidth>& refill_data,
            logic::Wire& refill_valid,
            logic::Wire& cache_write_enable,
            logic::Bus<CPUAddressWidth>& cache_line_address,
            logic::Wire& refill_done
        ): clock_(clock),
           reset_(reset),
           miss_(miss),
           miss_address_(miss_address),
           memory_address_(memory_address),
           memory_instruction_(memory_instruction), 
           cache_write_enable_(cache_write_enable),
           cache_line_address_(cache_line_address),
           cache_write_data()
          {}
    
        void evaluate() noexcept override;

        void refill(std::size_t index,std::size_t tag, const std::array<std::size_t, InstructionsPerLine>&data) noexcept;

        private:
         enum class State{
            IDLE,
            REFILL,
            INSTALL,
            COMPLETE
          };

          State state_;

          logic::Wire& clock_;
          logic::Wire& reset_;

          logic::Wire& miss_;
          logic::Bus<CPUAddressWidth>& miss_address_;

          logic::Bus<CPUAddressWidth>& memory_address_;
          logic::Bus<InstructionWidth>& memory_instruction_;

          logic::Bus<InstructionWidth>& refill_data_;
          logic::Wire& refill_valid_;

          logic::Wire& cache_write_enable_;
          logic::Bus<CPUAddressWidth>& cache_line_address_;

          std::array<logic::Bus<InstructionWidth>*,InstructionsPerLine>& cache_write_data_;
          logic::Wire& refill_done_;

          State state_ = State::IDLE;
          std::size_t refill_word_ = 0; 
          std::size_t miss_address_val_ =0;
          std::size_t line_base_address_ =0;
          std::array<std::size_t,InstructionsPerLine>refill_buffer_;

          /*
          Address conversion
          */
          static std::size_t bus_to_value(const logic::Bus<CPUAddressWidth>&bus) noexcept{
            std::size_t val = 0;

            for(std::size_t i=0;i<CPUAddressWidth;++i){
                if(bus[i].read() == logic::LogicState::HIGH){
                    value |= (1ULL << i);
                }
                return val;
            }
          }

          static void value_to_bus(std::size_t value,logic::Bus<CPUAddressWidth>&bus)noexcept{

            for(std::size_t i=0;i<CPUAddressWidth;++i){
                bus[i].write(
                    (value & (1ULL << i))
                        ? logic::LogicState::HIGH
                        : logic::LogicState::LOW
                );
            }
          }
        

    };
}