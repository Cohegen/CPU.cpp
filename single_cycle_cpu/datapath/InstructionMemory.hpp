/*
An implementation of instruction memory using ROM
since we're reading only
*/
#include <logic/sequential/memory/ROM.hpp>

namespace cpu{
    template<std::size_t AdressWidth,std::size_t InstructionWidth>
    class InstructionMemory{
        public:
           InstructionMemory(
            logic::Clock& clock,
            logic::Wire& reset,
            logic::Bus<AdressWidth>& address,
            logic::Bus<InstructionWidth>& instruction,
            const std::vector<std::size_t>& contents
           );

    }
}