/*
Control signals of the pipelined processor
*/
#include "components/ControlSignals.hpp"
namespace cpu{
   
    struct PipelinedControlSignals{
        //EX stage
        bool regDst= false;
        bool aluSrc= false;
        ALUOperation aluOp = ALUOperation::ADD;
    
        //MEM stage
        bool branch= false;
        bool memRead=false;
        bool memWrite= false;
    
        //WB stage
        bool regWrite=false;
        bool memToReg= false;
    };
}
