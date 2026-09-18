# Introduction to CPU.cpp
- This project intends to implement various CPU microarchitecture designs and execution models.
- The motivation of making this project, is to track and test my understanding in Computer Architecture and C++ programming as I learn them both.
- Here I implement various processors based on microarchitectural strategies and execution models.

## 1. Sequential Execution Models.
- These are the foundational models where instructions are processsed one after the other in  a strict sequential flow.
- Examples of them include:
  1. Single-cycle cpu : it executes one instruction per clock cycle.
  2. Multi-cycle cpu: here instructions are broken down into multiple smaller steps i.e fetch,decode,execute,writeback. Each step takes one clock cycle,allowing simpler instructions to finish faster than complex ones.

## 2. Instruction-Level Parallelism Models.
- This model overlaps or executes multiple instructions at the same time to boost speed.
- Examples of it include:
     1. Pipelined : it overlaps the execution of multiple instructions like an assembly line in say for example a factory. While one instruction is being executed, the next one is being decoded, and the one after that is being fetched.
     2. Supersaclar : duplicates internal execution units i.e like having multiple ALUs to execute multiple instructions completely in parallel during a single clock cycle.

## 3. Data-Level Parallelism Models
- This model is designed to crunch massive amounts of data by applying a single operation to large datasets simultaneously.
- Examples of them include:
    1. Vector : uses a single instruction to operate one-dimensional arrays of data i.e vectors using specialized, deeply pipelined vector registers.
    2. Array: uses a grid of multiple processing elements (PEs) that work in lockstep to manipulate to multiple multi-dimensional datasets simultaneously.
   
  
