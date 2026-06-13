# Instruction set — overview

This document is the entry point for the CPU-16 instruction set architecture (ISA). It covers the encoding formats and addressing modes in summary. Detailed opcode tables are in the subsection documents.

The ISA is **not yet fully defined**. This document will grow as design decisions are made.

---

## Design principles

The instruction set was designed with these priorities:

- **Compiler-friendly.** Symmetric registers, orthogonal addressing modes, and enough operations that a compiler does not need to synthesize everything from primitives.
- **Consistent encoding.** The same field means the same thing across all formats. The decoder has no special cases.
- **R0 is zero.** Because R0 always reads as zero, many pseudo-instructions come for free without special hardware.

---

## Instruction formats (summary)

Full specification in [`../architecture/cpu.md`](../architecture/cpu.md).

| Format | Width | Use |
|---|---|---|
| R | 16-bit | Register × register operations |
| I | 16-bit | Register + small immediate (6-bit, −32..+31) |
| L | 32-bit | Register + large immediate (16-bit, full range) |
| J | 16-bit | Unconditional jump with 12-bit PC-relative offset |
| B | 16-bit | Conditional branch with 9-bit PC-relative offset |

---

## Addressing modes (summary)

| Mode | Syntax | Notes |
|---|---|---|
| Register | `R1` | Direct register value |
| Immediate | `#42` | Literal constant |
| Absolute | `[0x1000]` | Fixed memory address |
| Register indirect | `[R1]` | Address in register |
| Register + offset | `[R1 + 4]` | Address + constant displacement |
| PC-relative | `[PC + offset]` | Position-independent |
| Register + register | `[R1 + R2]` | Array indexing |

---

## Instruction groups

| Group | Document | Status |
|---|---|---|
| Arithmetic — ADD, SUB, MUL, DIV, CMP | [`arithmetic.md`](arithmetic.md) | 🔴 Not yet written |
| Logic — AND, OR, XOR, NOT, shifts | [`logic.md`](logic.md) | 🔴 Not yet written |
| Memory — LOAD, STORE, MOV, PUSH, POP | [`memory.md`](memory.md) | 🔴 Not yet written |
| Control flow — JMP, CALL, RET, branches | [`control-flow.md`](control-flow.md) | 🔴 Not yet written |
| I/O — video, keyboard, system | [`io.md`](io.md) | 🔴 Not yet written |

---

## Pseudo-instructions

The assembler supports pseudo-instructions that expand to one or more real instructions. These are assembler conveniences, not hardware features.

| Pseudo | Expands to | Effect |
|---|---|---|
| `NOP` | `ADD R0, R0, R0` | No operation |
| `CLR Rx` | `MOV Rx, R0` | Clear register to zero |
| `MOV Rx, Ry` | `ADD Rx, Ry, R0` | Copy register |
| `INC Rx` | `ADD Rx, Rx, #1` | Increment by 1 |
| `DEC Rx` | `SUB Rx, Rx, #1` | Decrement by 1 |
| `BRZ Rx, label` | `BEQ Rx, R0, label` | Branch if Rx == 0 |
| `BNZ Rx, label` | `BNE Rx, R0, label` | Branch if Rx != 0 |
| `NOT Rx` | `XOR Rx, Rx, #-1` | Bitwise NOT |

This list will grow as the ISA is specified.

---

## Open decisions

- Multiply and divide: include in hardware or implement in software?
- Bit manipulation: dedicated instructions (BIT, SET, CLR bit) or use AND/OR masks?