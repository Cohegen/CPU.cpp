# Superscalar CPU

An ongoing implementation of a modern **multi-issue superscalar CPU architecture** designed to fetch, decode, and execute multiple instructions per cycle ($IPC > 1.0$).

> [!NOTE]
> **Work in Progress**: This architecture is currently under active development. Core fetch mechanisms (`FetchBundle`, `FetchUnit`) and the dual-port I-cache subsystem have been implemented and verified with comprehensive unit and integration test suites.

---

## Vision & Architecture Objectives

Standard pipelining is constrained by a theoretical limit of at most 1 instruction completed per cycle ($CPI = 1.0, IPC = 1.0$). Superscalar processors break this barrier by exploiting **Instruction-Level Parallelism (ILP)**:

- **2-Way In-Order Dual Issue**: Fetches, decodes, and issues up to 2 instructions per cycle.
- **Target IPC**: Up to **2.0 Instructions Per Cycle** ($CPI \approx 0.5$) for independent instruction streams.
- **Dynamic Structural & Data Hazard Detection**: Hardware interlocks detect RAW dependencies between simultaneous instructions and stall individual issue slots when necessary.

## Component Reuse Rule

Superscalar extends the existing CPU.cpp datapaths; it is not a separate
instruction-set simulator.  Components and ISA decoding are shared wherever
their hardware function is unchanged.  Width and throughput requirements are
met by instantiating multiple copies of a component, not by replacing it with
host-language arithmetic or a second decoder.

- `FetchUnit` reuses `ProgramCounter` and ripple-carry adders.
- `DecodeUnit` delegates instruction field decoding to `InstructionDecoder`.
- Each integer execution lane wraps the common `ALUInterface`; a two-wide core
  uses two lane instances.
- Structures unique to out-of-order execution (RAT, free list, issue queue,
  physical register file, and ROB) remain explicit architectural components
  with defined inputs, outputs, reset, and clock/enable behavior as they are
  integrated into the top-level datapath.

## Current Integer Execution Milestone

`IntegerExecutionPipeline` closes the integer vertical slice for up to two
ready ALU instructions per cycle: issue-queue selection, ALU execution,
physical-register write-back, dependent wake-up, ROB completion, and ordered
retirement. It intentionally defers branch recovery and load/store execution;
those need their own recovery and memory-ordering paths before the complete
core can run arbitrary programs.

---


