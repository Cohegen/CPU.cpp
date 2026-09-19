#pragma once

#include <cstdef>
#include <vector>

#include <logic/simulator/Component.hpp>
#include <logic/signals/bus.hpp>
#include <logic/signals/wire.hpp>
#include <logic/sequential/memory/DualPortROM.hpp>
namespace cpu{

    template<
    std::size_t CPUAddressWidth=32,
    std::size_t ROMAddressWidth=8,
    std::size_t InstructionWidth=32
    >
    class SuperScalarInstructionMemory: public logic::Component{
      static_assert(CPUAddressWidth >0, "CPUAddressWidth must be greater than zero!!");
      static_assert(ROMAddressWidth >0, "ROMAddressWidth must be greater than zero!!");
      static_assert(InstructionWidth >0, "InstructionWidth must be greater than zero!!");
      static_assert(InstructionWidth % 8 ==0,"InstructionWidth must be a multiple of 8!!");
      static_assert(
        (InstructionWidth / 8) && ((InstructionWidth/8)-1) == 0, "Instruction size in bytes must be a power of two"
      );
      static_assert(ROMAddressWidth <= CPUAddressWidth,"ROMAddressWidth cannot exceed CPUAddressWidth");


        public:
          static constexpr std::size_t InstructionBytes = InstructionWidth /8;

          static constexpr std::size_t AddressShift =[](){
            std::size_t value = InstructionBytes;
            std::size_t shift = 0;

            while(value >1){
              value >>=1;
              ++shift;
            }
            return shift;
          }();
          SuperScalarInstructionMemory(
            logic::Wire& enable,
            logic::Bus<CPUAddressWidth>& address0,
            logic::Bus<CPUAddressWidth>& address1,
            logic::Bus<InstructionWidth>& instruction0,
            logic::Bus<InstructionWidth>&instruction1,
            const std::vector<std::size_t>& contents = {}
          ): enable_(enable),
          address0_(address0),
          address1_(address1),
          instruction0_(instruction0),
          instruction1_(instruction1),
          rom_address0_{},
          rom_address1_{}, 
          rom_(
            enable_,
            rom_address0_,
            rom_address1_,
            instruction0_,
            instruction1_,
            contents
          )
            {}

          void evaluate() noexcept override{
            /*
             Converting byte addresses into instruction-word addresses
            */
            for(std::size_t i=0;i<ROMAddressWidth;++i){
              const std::size_t cpu_bit = i+ AddressShift;

              if(cpu_bit <CPUAddressWidth){
                rom_address0_[i].write(
                  address0_[cpu_bit].read();
                );

                rom_address1_[i].write(
                  address1_[cpu_bit].read()
                );
              }else{
                rom_address0_[i].write(logic::LogicState::LOW);

                rom_address1_[i].write(logic::LogicState::LOW);
              }
            }
            rom_.evaluate();
          }

          void load(const std::vector<std::size_t>& contents) noexcept{
            rom_.load_contents(contents);
          }

          void set_word(std::size_t index, std::size_t value) noexcept{
            rom_.set_word(index,value);
          }

          [[nodiscard]]
          std::size_t get_word(std::size_t index) const noexcept{
            return rom_.get_word(index);
          }

        private:
          //signals
          logic::Wire& enable_;
          //inputs
          logic::Bus<CPUAddressWidth>&address0_;
          logic::Bus<CPUAddressWidth>&address1_;

          //outputs
          logic::Bus<InstructionWidth>&instruction0_;
          logic::Bus<InstructionWidth>&instruction1_;

          std::vector<std::size_t>contents_;

          //internal buses
          logic::Bus<ROMAddressWidth> rom_address0_;
          logic::Bus<ROMAddressWidth> rom_address1_;

          logic::DualPortROM<ROMAddressWidth,InstructionWidth> rom_;

    };

}
