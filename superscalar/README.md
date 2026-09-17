# Superscalar CPU

An ongoing implementation of a modern **multi-issue superscalar CPU architecture** designed to fetch, decode, and execute multiple instructions per cycle ($IPC > 1.0$).

> [!NOTE]
> **Work in Progress**: This architecture is currently under active development. Core fetch mechanisms (`FetchBundle`, `FetchUnit`) have been laid down, with dual-issue decode, dependency checking, and execution pipelines actively being built.

---

## Vision & Architecture Objectives

Standard pipelining is constrained by a theoretical limit of at most 1 instruction completed per cycle ($CPI = 1.0, IPC = 1.0$). Superscalar processors break this barrier by exploiting **Instruction-Level Parallelism (ILP)**:

- **2-Way In-Order Dual Issue**: Fetches, decodes, and issues up to 2 instructions per cycle.
- **Target IPC**: Up to **2.0 Instructions Per Cycle** ($CPI \approx 0.5$) for independent instruction streams.
- **Dynamic Structural & Data Hazard Detection**: Hardware interlocks detect RAW dependencies between simultaneous instructions and stall individual issue slots when necessary.

---

## Current Status & Implemented Modules

Development is focused in the [`superscalar/core`](core/) directory:

### 1. `FetchBundle` ([`FetchBundle.hpp`](core/FetchBundle.hpp))
Encapsulates a bundle of instructions fetched simultaneously in a single clock cycle:
- `slot0`: Primary instruction word.
- `slot1`: Secondary instruction word (adjacent memory address).
- `slot0_valid` / `slot1_valid`: Validity flags indicating whether each slot contains a valid, executable instruction or was truncated/masked (e.g. by branch redirection or unaligned targets).

### 2. `FetchUnit` ([`FetchUnit.hpp`](core/FetchUnit.hpp))
Implements 2-way wide instruction fetch logic:
- Fetches adjacent 32-bit instruction words (`PC` and `PC + 1`) from dual-ported instruction memory.
- Advances PC by `+2` during normal dual-issue flow, or by `+1` if a single instruction was consumed.
- Supports stall signals from subsequent pipeline stages (decode/hazard unit).

---

## Roadmap & Next Milestones

- [x] **2-Way Fetch Unit & Instruction Bundle** (`FetchBundle`, `FetchUnit`)
- [ ] **Dual Instruction Decoder**: Decode both `slot0` and `slot1` concurrently.
- [ ] **Issue & Inter-Slot Dependency Checker**: Detect RAW hazards between `slot0` (producing register) and `slot1` (consuming same register in same cycle), falling back to single-issue when dependent.
- [ ] **Dual Execution Datapaths**: Dual ALU functional units and memory arbitration.
- [ ] **Multi-Ported Register File**: 4 read ports and 2 write ports to support dual-issue execution.
- [ ] **Superscalar Hazard & Forwarding Unit**: Forwarding results across both execution pipelines.
- [ ] **Benchmarking & CPI Analysis**: Comparison against Single-Cycle, Multi-Cycle, and Pipelined baselines in Python.
