# Changelog

All notable changes to the CPU.cpp repository will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [2.0.0] - 2026-10-17

### Added
- **2-Wide Out-of-Order Superscalar CPU** (`superscalar/`):
  - **Frontend**: 2-wide parallel `FetchUnit`, dual-port `SuperScalarInstructionMemory`, `FetchDecodeReg`, and dual-issue `DecodeUnit`.
  - **Register Renaming**: Register Alias Table (`RegisterAliasTable`), Physical Register Free List (`PhysicalRegisterFreeList`), and 64-entry Physical Register File (`PhysicalRegisterFile`).
  - **Dispatch & Out-of-Order Issue**: 16-entry Reorder Buffer (`ReOrderBuffer`), 16-entry associative `IssueQueue` with tag wakeup and oldest-ready select logic.
  - **Execution Engine**: Parallel `ALUExecutionUnit` and `IntegerExecutionPipeline`.
  - **Dynamic Branch Prediction**: 256-entry 2-bit saturating counter `BranchPredictor`, `BranchExecutionUnit`, and single-cycle RAT snapshot `BranchRecoveryUnit`.
  - **Commit & Retirement**: In-order `CommitUnit` retiring up to 2 instructions per cycle, with dedicated speculative `StoreBuffer`.
  - **Memory Disambiguation**: 32-entry `LoadStoreQueue` with associative store-to-load forwarding and `MemoryExecutionUnit`.
  - **Data Cache (D-Cache)**: Configurable write-allocate, write-back `DataCache` with dirty line eviction and flush mechanism.
- **Top-Level Wrapper**: `cpu::SuperScalarCPU` in `components/SuperScalarCPU.hpp` with unified interface matching scalar CPUs.
- **Microarchitectural Benchmark Suite**: `tests/superscalar/SuperScalarBenchmarkTests.cpp` demonstrating superscalar throughput advantage ($\text{IPC} = 1.12 > 1.0$), iterative Fibonacci sequence, and memory forwarding.
- **Python Bindings (`pycpu`)**:
  - `NativeSuperScalarCPU32` exported in `_pycpu_core`.
  - `CPU(architecture="superscalar")` with `ipc` and `branch_accuracy` properties.
  - Architecture-aware relative branch offset and 4-byte PC stepping in `pycpu.assembler`.
- **Interactive Sidebar Dashboard**: `cpu_benchmark_dashboard.html` for real-time visualization of cross-architecture metrics.

---

## [1.0.0] - 2026-09-18

### Added
- **Single-Cycle CPU** (`single_cycle_cpu/`): Baseline 32-bit RISC processor with combinational datapath and CPI = 1.0.
- **Multi-Cycle CPU** (`multi_cycle_cpu/`): Resource-shared architecture driven by an FSM Control Unit (3–5 cycles/instruction).
- **5-Stage Pipelined CPU** (`pipelined_cpu/`): Classic IF, ID, EX, MEM, WB pipeline with EX/MEM hazard forwarding and branch stall handling.
- **Unified Wrappers & ISA**: `include/isa/Opcode.hpp`, `include/isa/Registers.hpp`, and wrapper headers in `components/`.
- **Python Integration (`pycpu`)**: Python C++ extension using pybind11 with two-pass assembler and benchmark suite.
