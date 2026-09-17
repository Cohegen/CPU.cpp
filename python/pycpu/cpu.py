from typing import Union, List, Dict, Any
from ._pycpu_core import (
    Register,
    Opcode,
    NativeSingleCycleCPU32,
    NativeMultiCycleCPU32,
    NativePipelinedCPU32,
)
from .assembler import assemble, _REG_MAP


class RegisterAccessor:
    """Dictionary-like accessor for CPU general purpose registers (r0-r15)."""

    def __init__(self, native_cpu: Any):
        self._cpu = native_cpu

    def _resolve_reg(self, key: Union[int, str, Register]) -> Register:
        if isinstance(key, Register):
            return key
        if isinstance(key, int):
            if 0 <= key <= 15:
                return getattr(Register, f"R{key}")
            raise IndexError(f"Register index {key} out of range [0, 15]")
        key_str = str(key).strip().lower()
        if key_str in _REG_MAP:
            return _REG_MAP[key_str]
        raise KeyError(f"Unknown register name: '{key}'. Valid names: r0-r15, x0-x15, zero")

    def __getitem__(self, key: Union[int, str, Register]) -> int:
        reg = self._resolve_reg(key)
        return self._cpu.read_register(reg)

    def __setitem__(self, key: Union[int, str, Register], value: int):
        reg = self._resolve_reg(key)
        self._cpu.write_register(reg, int(value) & 0xFFFFFFFF)

    def to_dict(self) -> Dict[str, int]:
        return {f"r{i}": self[i] for i in range(16)}

    def __repr__(self) -> str:
        items = [f"r{i}={self[i]}" for i in range(16) if self[i] != 0 or i < 4]
        return f"<Registers {', '.join(items)}>"


class MemoryAccessor:
    """Dictionary-like accessor for CPU data memory."""

    def __init__(self, native_cpu: Any):
        self._cpu = native_cpu

    def __getitem__(self, address: int) -> int:
        return self._cpu.read_memory(int(address))

    def __setitem__(self, address: int, value: int):
        self._cpu.write_memory(int(address), int(value) & 0xFFFFFFFF)

    def to_dict(self, start: int = 0x80, count: int = 16) -> Dict[str, int]:
        return {hex(addr): self[addr] for addr in range(start, start + count)}

    def __repr__(self) -> str:
        non_zero = {hex(addr): self[addr] for addr in range(0x80, 0xA0) if self[addr] != 0}
        return f"<Memory {non_zero}>"


class CPU:
    """High-level Pythonic CPU simulation frontend wrapping C++ CPU.cpp.

    Supports microarchitectures:
      - 'single_cycle' (default): Single-Cycle CPU
      - 'multi_cycle': Autonomous Multi-Cycle CPU with FSM controller
      - 'pipelined': 5-stage Pipelined CPU with hazard handling & forwarding
    """

    SUPPORTED_ARCHITECTURES = ("single_cycle", "multi_cycle", "pipelined")

    def __init__(self, architecture: str = "single_cycle"):
        norm_arch = architecture.strip().lower().replace("-", "_")
        if norm_arch not in self.SUPPORTED_ARCHITECTURES:
            raise ValueError(
                f"Unknown architecture '{architecture}'. Supported: {self.SUPPORTED_ARCHITECTURES}"
            )

        self._architecture = norm_arch

        if norm_arch == "single_cycle":
            self._native = NativeSingleCycleCPU32()
        elif norm_arch == "multi_cycle":
            self._native = NativeMultiCycleCPU32()
        elif norm_arch == "pipelined":
            self._native = NativePipelinedCPU32()

        self.registers = RegisterAccessor(self._native)
        self.memory = MemoryAccessor(self._native)

    @property
    def architecture(self) -> str:
        """The microarchitecture model of this CPU."""
        return self._architecture

    @property
    def native(self) -> Any:
        return self._native

    def reset(self):
        """Resets the CPU state and PC to 0."""
        self._native.reset()

    def step(self):
        """Advances simulation by one clock cycle."""
        self._native.step()

    def step_instruction(self):
        """Advances until the current instruction completes (Multi-cycle only)."""
        if hasattr(self._native, "step_instruction"):
            self._native.step_instruction()
        else:
            self.step()

    def run(self, max_cycles: int = 100000) -> int:
        """Runs until a HALT instruction is encountered or max_cycles is reached.

        Returns elapsed cycles during this run.
        """
        start_cycles = self.cycles
        self._native.run(max_cycles)
        return self.cycles - start_cycles

    @property
    def pc(self) -> int:
        """Current program counter."""
        return self._native.pc()

    @property
    def is_halted(self) -> bool:
        """Whether execution stopped at a HALT instruction."""
        return self._native.halted()

    @property
    def cycles(self) -> int:
        """Number of elapsed clock cycles."""
        return self._native.cycles()

    def cpi(self, num_instructions: int) -> float:
        """Calculates Cycles Per Instruction (CPI)."""
        if num_instructions <= 0:
            return 0.0
        return self.cycles / num_instructions

    def read_memory(self, address: int) -> int:
        """Reads a 32-bit word from data memory at address."""
        return self._native.read_memory(address)

    def write_memory(self, address: int, value: int):
        """Writes a 32-bit word into data memory at address."""
        self._native.write_memory(address, value)

    def load_data(self, values: List[int], start_address: int = 0x80):
        """Loads an array of 32-bit values into data memory starting at start_address."""
        for i, val in enumerate(values):
            self.write_memory(start_address + i, val)

    def load_program(self, instructions: List[int]):
        """Loads raw 32-bit machine instructions into instruction memory."""
        self._native.load_program(instructions)

    def load_assembly(self, asm_code: str) -> List[int]:
        """Assembles and loads human-readable assembly instructions with architecture-specific resolution."""
        words = assemble(asm_code, architecture=self.architecture)
        self.load_program(words)
        return words

    def inspect(self) -> Dict[str, Union[int, bool, str, Dict[str, int]]]:
        """Returns a snapshot of the CPU state."""
        return {
            "architecture": self.architecture,
            "pc": self.pc,
            "cycles": self.cycles,
            "halted": self.is_halted,
            "registers": self.registers.to_dict(),
            "memory": self.memory.to_dict(),
        }

    def dump_state(self) -> str:
        """Returns a human-readable formatted string of the CPU state."""
        lines = [
            f"=== CPU State [{self.architecture}] ===",
            f"PC: {self.pc} | Cycles: {self.cycles} | Halted: {self.is_halted}",
            "--- Registers ---",
        ]
        reg_chunks = []
        for i in range(16):
            val = self.registers[i]
            reg_chunks.append(f"r{i:02d}: 0x{val:08X} ({val})")
            if len(reg_chunks) == 4:
                lines.append("  " + " | ".join(reg_chunks))
                reg_chunks = []
        if reg_chunks:
            lines.append("  " + " | ".join(reg_chunks))

        non_zero_mem = [
            (addr, self.read_memory(addr))
            for addr in range(0x80, 0x100)
            if self.read_memory(addr) != 0
        ]
        if non_zero_mem:
            lines.append("--- RAM (Non-zero words in 0x80..0xFF) ---")
            for addr, val in non_zero_mem:
                lines.append(f"  [0x{addr:02X}]: 0x{val:08X} ({val})")
        return "\n".join(lines)

    def __repr__(self) -> str:
        return (
            f"<CPU arch={self.architecture} pc={self.pc} cycles={self.cycles} halted={self.is_halted}>"
        )
