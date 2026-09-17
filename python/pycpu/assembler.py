import re
from typing import List, Dict, Optional, Union, Tuple
from ._pycpu_core import (
    Opcode,
    Register,
    encode_r_type,
    encode_i_type,
    encode_s_type,
    encode_b_type,
    encode_j_type,
)

_REG_MAP = {
    "r0": Register.R0, "x0": Register.R0, "zero": Register.R0,
    "r1": Register.R1, "x1": Register.R1,
    "r2": Register.R2, "x2": Register.R2,
    "r3": Register.R3, "x3": Register.R3,
    "r4": Register.R4, "x4": Register.R4,
    "r5": Register.R5, "x5": Register.R5,
    "r6": Register.R6, "x6": Register.R6,
    "r7": Register.R7, "x7": Register.R7,
    "r8": Register.R8, "x8": Register.R8,
    "r9": Register.R9, "x9": Register.R9,
    "r10": Register.R10, "x10": Register.R10,
    "r11": Register.R11, "x11": Register.R11,
    "r12": Register.R12, "x12": Register.R12,
    "r13": Register.R13, "x13": Register.R13,
    "r14": Register.R14, "x14": Register.R14,
    "r15": Register.R15, "x15": Register.R15,
}

def _parse_reg(token: str) -> Register:
    token = token.strip().lower()
    if token in _REG_MAP:
        return _REG_MAP[token]
    # Allow raw integer index like 1 -> Register.R1
    if token.isdigit():
        idx = int(token)
        if 0 <= idx <= 15:
            return _REG_MAP[f"r{idx}"]
    raise ValueError(f"Unknown register name: '{token}'. Valid registers: r0-r15 / x0-x15 / zero")

def _parse_int(token: str) -> int:
    token = token.strip()
    if token.startswith(("0x", "0X")):
        return int(token, 16)
    if token.startswith(("0b", "0B")):
        return int(token, 2)
    return int(token)


def _strip_comments(line: str) -> str:
    return re.sub(r"[;#].*$", "", line).strip()


def assemble_instruction(
    line: str,
    current_pc: int = 0,
    symbols: Optional[Dict[str, int]] = None,
    architecture: str = "single_cycle",
) -> Optional[int]:
    """Assembles a single instruction statement with symbol table and architecture awareness."""
    symbols = symbols or {}
    line = _strip_comments(line)
    if not line:
        return None

    # Check for pseudo-instructions first
    parts = re.split(r"[\s,]+", line)
    op = parts[0].upper()
    args = parts[1:]

    # Pseudo-instruction: mv rd, rs -> add rd, rs, r0
    if op == "MV":
        if len(args) != 2:
            raise ValueError(f"mv expects 2 operands (rd, rs), got {len(args)}: '{line}'")
        rd = _parse_reg(args[0])
        rs = _parse_reg(args[1])
        return encode_r_type(Opcode.ADD, rd, rs, Register.R0)

    # Pseudo-instruction: beqz rs, target -> beq rs, r0, target
    elif op == "BEQZ":
        if len(args) != 2:
            raise ValueError(f"beqz expects 2 operands (rs, target), got {len(args)}: '{line}'")
        rs = _parse_reg(args[0])
        target_token = args[1]
        if target_token in symbols:
            target_pc = symbols[target_token]
            imm = target_pc - current_pc if architecture == "single_cycle" else target_pc - (current_pc + 1)
        else:
            imm = _parse_int(target_token)
        return encode_b_type(Opcode.BEQ, rs, Register.R0, imm & 0x3FFFF)

    # Pseudo-instruction: bnez rs, target -> bne rs, r0, target
    elif op == "BNEZ":
        if len(args) != 2:
            raise ValueError(f"bnez expects 2 operands (rs, target), got {len(args)}: '{line}'")
        rs = _parse_reg(args[0])
        target_token = args[1]
        if target_token in symbols:
            target_pc = symbols[target_token]
            imm = target_pc - current_pc if architecture == "single_cycle" else target_pc - (current_pc + 1)
        else:
            imm = _parse_int(target_token)
        return encode_b_type(Opcode.BNE, rs, Register.R0, imm & 0x3FFFF)

    # Pseudo-instruction: b target -> beq r0, r0, target
    elif op == "B":
        if len(args) != 1:
            raise ValueError(f"b expects 1 operand (target), got {len(args)}: '{line}'")
        target_token = args[0]
        if target_token in symbols:
            target_pc = symbols[target_token]
            imm = target_pc - current_pc if architecture == "single_cycle" else target_pc - (current_pc + 1)
        else:
            imm = _parse_int(target_token)
        return encode_b_type(Opcode.BEQ, Register.R0, Register.R0, imm & 0x3FFFF)

    elif op == "NOP":
        return encode_r_type(Opcode.NOP, Register.R0, Register.R0, Register.R0)

    elif op == "HALT":
        return encode_r_type(Opcode.HALT, Register.R0, Register.R0, Register.R0)

    elif op in ("ADD", "SUB", "AND", "OR", "XOR"):
        opcode = getattr(Opcode, op)
        rd = _parse_reg(args[0])
        rs1 = _parse_reg(args[1])
        rs2 = _parse_reg(args[2])
        return encode_r_type(opcode, rd, rs1, rs2)

    elif op == "NOT":
        rd = _parse_reg(args[0])
        rs1 = _parse_reg(args[1])
        return encode_r_type(Opcode.NOT, rd, rs1, Register.R0)

    elif op == "ADDI":
        rd = _parse_reg(args[0])
        rs1 = _parse_reg(args[1])
        imm = _parse_int(args[2])
        return encode_i_type(Opcode.ADDI, rd, rs1, imm & 0x3FFFF)

    elif op == "LI":
        rd = _parse_reg(args[0])
        imm = _parse_int(args[1])
        return encode_i_type(Opcode.LI, rd, Register.R0, imm & 0x3FFFF)

    elif op == "LW":
        rd = _parse_reg(args[0])
        if "(" in args[1]:
            match = re.match(r"(-?\w+)\(([A-Za-z0-9_]+)\)", args[1])
            if match:
                imm = _parse_int(match.group(1))
                rs1 = _parse_reg(match.group(2))
            else:
                raise ValueError(f"Malformed memory operand: {args[1]}")
        else:
            rs1 = _parse_reg(args[1])
            imm = _parse_int(args[2]) if len(args) > 2 else 0
        return encode_i_type(Opcode.LW, rd, rs1, imm & 0x3FFFF)

    elif op == "SW":
        rs2 = _parse_reg(args[0])
        if "(" in args[1]:
            match = re.match(r"(-?\w+)\(([A-Za-z0-9_]+)\)", args[1])
            if match:
                imm = _parse_int(match.group(1))
                rs1 = _parse_reg(match.group(2))
            else:
                raise ValueError(f"Malformed memory operand: {args[1]}")
        else:
            rs1 = _parse_reg(args[1])
            imm = _parse_int(args[2]) if len(args) > 2 else 0
        return encode_s_type(Opcode.SW, rs2, rs1, imm & 0x3FFFF)

    elif op in ("BEQ", "BNE"):
        opcode = getattr(Opcode, op)
        rs1 = _parse_reg(args[0])
        rs2 = _parse_reg(args[1])
        target_token = args[2]
        if target_token in symbols:
            target_pc = symbols[target_token]
            imm = target_pc - current_pc if architecture == "single_cycle" else target_pc - (current_pc + 1)
        else:
            imm = _parse_int(target_token)
        return encode_b_type(opcode, rs1, rs2, imm & 0x3FFFF)

    elif op == "J":
        target_token = args[0]
        if target_token in symbols:
            imm = symbols[target_token]
        else:
            imm = _parse_int(target_token)
        return encode_j_type(Opcode.J, imm & 0x03FFFFFF)

    else:
        raise ValueError(f"Unknown instruction opcode: '{op}' in '{line}'")


def assemble_line(line: str) -> Optional[int]:
    """Assembles a single line of assembly (backwards compatibility)."""
    return assemble_instruction(line)


def assemble(
    asm_text: Union[str, List[str]],
    architecture: str = "single_cycle",
) -> List[int]:
    """Two-pass assembler translating assembly source into 32-bit machine code.

    Supports:
      - Labels: standalone (e.g. 'loop:') or inline (e.g. 'loop: addi r1, r1, 1')
      - Branch/jump label resolution with architecture-specific offset handling
      - Pseudo-instructions: mv, beqz, bnez, li, nop, halt
      - Hex/binary/decimal immediates
      - Comments (# and ;)
    """
    raw_lines = asm_text.strip().splitlines() if isinstance(asm_text, str) else asm_text

    # Pass 1: Parse labels and collect instructions with their source line and assigned PC
    symbols: Dict[str, int] = {}
    instructions: List[Tuple[int, int, str]] = []  # (pc, line_num, stmt)
    current_pc = 0

    for line_num, line in enumerate(raw_lines, start=1):
        cleaned = _strip_comments(line)
        if not cleaned:
            continue

        # Check for label definition: 'label:' at start of string
        while ":" in cleaned:
            match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(.*)$", cleaned)
            if not match:
                break
            label_name = match.group(1)
            cleaned = match.group(2).strip()
            if label_name in symbols:
                raise ValueError(f"Duplicate label definition '{label_name}' at line {line_num}")
            symbols[label_name] = current_pc

        if cleaned:
            instructions.append((current_pc, line_num, cleaned))
            current_pc += 1

    # Pass 2: Assemble instructions with symbol table
    norm_arch = architecture.strip().lower().replace("-", "_")
    machine_code: List[int] = []
    for pc, line_num, stmt in instructions:
        try:
            word = assemble_instruction(
                stmt,
                current_pc=pc,
                symbols=symbols,
                architecture=norm_arch,
            )
            if word is not None:
                machine_code.append(word)
        except Exception as e:
            raise ValueError(f"Assembly error at line {line_num} ('{stmt}'): {e}") from e

    return machine_code
