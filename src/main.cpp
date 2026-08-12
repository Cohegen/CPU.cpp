#include <iostream>

#include "combinational/adders/HalfAdder.hpp"
#include "signals/logicState.hpp"
#include "signals/wire.hpp"

int main()
{
    logic::Wire a{logic::LogicState::HIGH};
    logic::Wire b{logic::LogicState::HIGH};
    logic::Wire sum;
    logic::Wire carry;

    logic::HalfAdder halfAdder(a, b, sum, carry);
    halfAdder.evaluate();

    std::cout << "SUM: " << sum.read() << '\n';
    std::cout << "CARRY: " << carry.read() << '\n';

    return sum.read() == logic::LogicState::LOW &&
           carry.read() == logic::LogicState::HIGH
        ? 0
        : 1;
}
