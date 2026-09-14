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
        raise KeyError(f"Unknown register name: '{key}'. Valid names: r0-r15, x0-x15")

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

    def load_program(self, instructions: List[int]):
        """Loads raw 32-bit machine instructions into instruction memory."""
        self._native.load_program(instructions)

    def load_assembly(self, asm_code: str) -> List[int]:
        """Assembles and loads human-readable assembly instructions."""
        words = assemble(asm_code)
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
        }

    def __repr__(self) -> str:
        return (
            f"<CPU arch={self.architecture} pc={self.pc} cycles={self.cycles} halted={self.is_halted}>"
        )
