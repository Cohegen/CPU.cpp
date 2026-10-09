namespace cpu {
    struct ExecutionRequest
    {
        bool valid{false};
        std::uint32_t pc{};
        std::uint32_t instruction{};

        Opcode opcode{Opcode::NOP};

        PhysicalRegister physical_rs1{};
        PhysicalRegister physical_rs2{};
        PhysicalRegister physical_rd{};

        std::uint32_t rs1_value{};
        std::uint32_t rs2_value{};
        std::int32_t immediate{};

        ALUOperation alu_operation{ALUOperation::NONE};
        OperandSource operand_a{OperandSource::NONE};
        OperandSource operand_b{OperandSource::NONE};

        bool memory_read{false};
        bool memory_write{false};

        ControlFlow control_flow{ControlFlow::NONE};
        std::size_t rob_index{};

        bool halt{false};

        //predicted information
        bool predicted_taken{false};
        std::uint32_t predicted_target{};
    };

  

}