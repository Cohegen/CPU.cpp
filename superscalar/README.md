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

- Work is still underway. 
