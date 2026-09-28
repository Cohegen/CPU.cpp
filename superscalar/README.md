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

---

## Current Status & Implemented Modules

### 1. Core Fetch Modules
- **`FetchBundle`** ([`FetchBundle.hpp`](core/FetchBundle.hpp)):
  Encapsulates the dual-issue fetch bundle output, containing `(PC0, Instruction0, Valid0)` for Slot 0 and `(PC1, Instruction1, Valid1)` for Slot 1.
- **`FetchUnit`** ([`FetchUnit.hpp`](core/FetchUnit.hpp)):
  Sequential 2-wide fetch engine. Automatically advances the architectural Program Counter by 8 bytes per cycle, generating concurrent fetch addresses:
  - $\text{Lane 0} = \text{PC}$
  - $\text{Lane 1} = \text{PC} + 4$
  - $\text{Next PC} = \text{PC} + 8$
  Supports an optional hardware `stall` signal to freeze PC progression during instruction cache miss refills, and handles valid suppression during reset.
- **`ProgramCounter`** ([`ProgramCounter.hpp`](../components/ProgramCounter.hpp)):
  Parameterized N-bit sequential program counter register with enable and synchronous reset control.

### 2. Instruction Cache & Memory Subsystem
- **`InstructionCacheLine`** ([`CacheLine.hpp`](core/CacheLine.hpp)):
  Multi-line, dual-read-port instruction cache with direct-mapped tag matching and independent dual lookups per cycle.
- **`InstructionCacheController`** ([`InstructionCacheController.hpp`](core/InstructionCacheController.hpp)):
  Hardware FSM controller (`IDLE` $\rightarrow$ `REFILL_REQUEST` $\rightarrow$ `REFILL_WAIT` $\rightarrow$ `INSTALL` $\rightarrow$ `COMPLETE`) coordinating cache-line refills from memory.
- **`SuperscalarInstructionMemory`** ([`SuperScalarInstructionMemory.hpp`](core/SuperScalarInstructionMemory.hpp)):
  Dual-port ROM instruction store.

---

## Verification & Test Suite

The superscalar subsystem is validated with comprehensive unit and integration tests:

| Test Target | Type | Description | Status |
| :--- | :--- | :--- | :--- |
| `test_fetch_unit` | Unit | Tests `FetchUnit` reset suppression, sequential dual-issue advancement (+8 bytes/cycle), hit/valid signal propagation, bundle packaging, stall freezing, and mid-stream reset. | **PASSED** |
| `test_cache_line` | Unit | Tests direct-mapped tag, index, offset splitting, cold misses, hit delivery, offset decoding, alias tag mismatches, dual-port concurrent hits/misses, and cache invalidation. | **PASSED** |
| `test_memory_refill_integration` | Integration | Validates controller state machine cycle-by-cycle refill sequence and full cache line installation. | **PASSED** |
| `test_fetch_icache_integration` | Integration | End-to-end integration of `FetchUnit` + `InstructionCacheLine` + `InstructionCacheController` + `SuperscalarInstructionMemory`: cold misses, in-line zero-stall hits ($IPC = 2.0$), multi-line transitions, dual-port cross-line lookups, and continuous program stream fetch. | **PASSED** |

### Running Superscalar Tests

```powershell
# Ensure compiler toolchain is in PATH
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

# Run all superscalar tests via CTest
ctest --test-dir build -R "cache_line|refill|fetch" --output-on-failure
```
