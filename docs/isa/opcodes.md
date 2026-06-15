# Opcode table

This document is the authoritative reference for the CPU-16 binary encoding. Every instruction is listed with its opcode, format, width, and sub-encoding fields. The emulator decoder should implement this table exactly.

---

## Opcode map

| Opcode | Mnemonic | Format | Width | Description |
|--------|----------|--------|-------|-------------|
| 0x0 | ALU | R | 16-bit | Register ALU — func[2:0] selects operation |
| 0x1 | ADDI | I | 16-bit | Rd = Ra + sign_extend(imm6) |
| 0x2 | SHIFT | R | 16-bit | Shift/rotate by register — func[1:0] selects type |
| 0x3 | SHIFTI | I | 16-bit | Shift/rotate by immediate — imm[5:4] = type, imm[3:0] = amount |
| 0x4 | ALUL | L | 32-bit | ALU with 16-bit immediate — mode[2:0] selects operation |
| 0x5 | LOAD | M | 16/32-bit | Load — mode[5] determines width (see below) |
| 0x6 | STORE | M | 16/32-bit | Store — same mode encoding as LOAD |
| 0x7 | STACK | R | 16-bit | PUSH (func=0) / POP (func=1) |
| 0x8 | SPREG | R | 16-bit | Special register access — func[2:0] selects operation |
| 0x9 | JMP | J | 16-bit | JMP PC + 2 + offset12×2 |
| 0xA | JMPX | R | 16-bit | JMP Rs / CALL Rs / RET / IRET — func[2:0] selects |
| 0xB | JMPL | L | 32-bit | JMP #addr16 / CALL #addr16 — mode[0] selects |
| 0xC | BCC | B | 16-bit | Conditional branch — cond[2:0] selects condition |
| 0xD | SYS | S | 16-bit | HALT / STI / CLI / TRAP #n — func[2:0] selects |
| 0xE | — | — | — | Reserved |
| 0xF | — | — | — | Reserved |

---

## Instruction width rule

The decoder reads one 16-bit word. It is a 32-bit instruction (and must fetch a second word) if and only if the opcode is **0x4**, **0xB**, or **0x5/0x6 with mode[5] = 0** (see LOAD/STORE below). Every other opcode is always 16 bits.

---

## 0x0 — ALU (Format R, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3 |  2   1   0
[  0x0  (4)   ] [  Rd (3)  ] [  Ra (3)  ] [  Rb (3)  ] [ func (3) ]
```

| func[2:0] | Operation | Result | Flags |
|-----------|-----------|--------|-------|
| 000 | ADD | Rd = Ra + Rb | Z N C V |
| 001 | SUB | Rd = Ra − Rb | Z N C V |
| 010 | MUL | Rd = (Ra × Rb)[15:0] | Z N (C/V undefined) |
| 011 | AND | Rd = Ra & Rb | Z N (C=0 V=0) |
| 100 | OR  | Rd = Ra \| Rb | Z N (C=0 V=0) |
| 101 | XOR | Rd = Ra ^ Rb | Z N (C=0 V=0) |
| 110 | NOT | Rd = ~Ra — Rb ignored | Z N (C=0 V=0) |
| 111 | CMP | flags from Ra − Rb — Rd ignored (use R0) | Z N C V |

---

## 0x1 — ADDI (Format I, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3   2   1   0
[  0x1  (4)   ] [  Rd (3)  ] [  Ra (3)  ] [       imm6 (6)       ]
```

Rd = Ra + sign_extend(imm6). Immediate range: −32..+31.

Flags: Z N C V (same as ADD).

**ADDI is the only 16-bit immediate ALU instruction.** To do SUB, AND, OR, XOR, MUL, or CMP with a constant, use ALUL (opcode 0x4).

Common uses:
- `ADDI Rd, Ra, #n` — add small constant
- `ADDI Rd, Ra, #-n` — subtract small constant (note: C flag reflects addition carry, not subtraction borrow — do not use BLO/BHS after ADDI for unsigned comparisons)
- `ADDI Rd, R0, #n` — load small constant into Rd (assembler pseudo: `MOV Rd, #n`)
- `ADDI R0, Ra, #-n` — compare Ra against n using Z/N flags only (assembler pseudo: `CMP Ra, #n` for signed-only branches)

---

## 0x2 — SHIFT (Format R, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3 |  2   1   0
[  0x2  (4)   ] [  Rd (3)  ] [  Ra (3)  ] [  Rb (3)  ] [0 | t (2)]
```

- `t[1:0]` — shift type (see table below)
- `func[2]` — reserved, must be 0
- Shift amount: Rb[3:0] (low 4 bits; upper 12 bits of Rb are ignored)

| t[1:0] | Operation |
|--------|-----------|
| 00 | LSL — logical shift left |
| 01 | LSR — logical shift right (zero fill) |
| 10 | ASR — arithmetic shift right (sign fill) |
| 11 | ROR — rotate right |

Flags: Z N C (last bit shifted/rotated out). V undefined.

---

## 0x3 — SHIFTI (Format I, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4 |  3   2   1   0
[  0x3  (4)   ] [  Rd (3)  ] [  Ra (3)  ] [ t (2) ] [  amount (4) ]
```

- `t[1:0]` — shift type (same as SHIFT above)
- `amount[3:0]` — shift amount, 0..15

Flags: same as SHIFT.

---

## 0x4 — ALUL (Format L, 32-bit)

```
Word 1:  15  14  13  12 | 11  10   9 |  8   7   6 |  5  ..  3 |  2   1   0
        [  0x4  (4)   ] [  Rd (3)  ] [  Ra (3)  ] [ 000 (3)  ] [ op  (3) ]
Word 2: [                          imm16 (16)                              ]
```

- `op[2:0]` — selects the ALU operation
- `mode[5:3]` — reserved, must be 0
- Word 2 — 16-bit immediate (signed for ADD/SUB/CMP; value for AND/OR/XOR/MUL)

| op[2:0] | Operation | Notes |
|---------|-----------|-------|
| 000 | ADD | Rd = Ra + imm16 |
| 001 | SUB | Rd = Ra − imm16 |
| 010 | AND | Rd = Ra & imm16 |
| 011 | OR  | Rd = Ra \| imm16 |
| 100 | XOR | Rd = Ra ^ imm16 |
| 101 | MUL | Rd = (Ra × imm16)[15:0] |
| 110 | CMP | flags from Ra − imm16; Rd ignored (use R0) |
| 111 | reserved | — |

Flags: same as ALU (0x0) for the corresponding operation.

---

## 0x5 / 0x6 — LOAD / STORE (Format M, 16 or 32-bit)

LOAD and STORE share one format. For STORE, the `Rd` field encodes the **source register** (the value to write); Ra encodes the base address.

```
Word 1:  15  14  13  12 | 11  10   9 |  8   7   6 |  5 |  4   3   2   1   0
        [  op   (4)   ] [ Rd/Rs (3) ] [  Ra  (3)  ] [W ] [        X       ]
```

- `W` (bit 5) — **width selector**: 1 = 16-bit instruction, 0 = 32-bit instruction (word 2 follows)
- `X` (bits 4–0) — meaning depends on W:

### W = 1 (16-bit LOAD/STORE)

Address = Ra + sign_extend(X[4:0]). Displacement range: **−16..+15 bytes**.

```
LOAD  Rd, [Ra + #disp5]    ; disp5 = sign_extend(X[4:0])
STORE [Ra + #disp5], Rs
```

`[Ra]` (no offset) encodes as disp5 = 0.

### W = 0 (32-bit LOAD/STORE)

Word 2 follows. X[3:0] selects the addressing mode:

| X[3:0] | Mode | Word 2 |
|--------|------|--------|
| 0000 | [#imm16] absolute | 16-bit target address; Ra ignored |
| 0001 | [Ra + #imm16] offset | 16-bit signed byte offset |
| 0010 | [PC + #imm16] PC-relative | 16-bit signed byte offset from PC+4 |
| 0011 | [Ra + Rb] reg+reg | Word 2 bits [2:0] = Rb; bits [15:3] ignored |
| 0100–1111 | reserved | — |

X[4] is reserved and must be 0 in the 32-bit case.

---

## 0x7 — STACK (Format R, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3 |  2   1   0
[  0x7  (4)   ] [  Rd (3)  ] [  Ra (3)  ] [  000 (3)  ] [0 | 0 | f]
```

- `f` (bit 0) — 0 = PUSH, 1 = POP
- Bits 2–1 reserved, must be 0

| f | Operation | Register used |
|---|-----------|---------------|
| 0 | PUSH | Ra — source (Rd ignored) |
| 1 | POP  | Rd — destination (Ra ignored) |

Neither PUSH nor POP affects flags.

---

## 0x8 — SPREG (Format R, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3 |  2   1   0
[  0x8  (4)   ] [  Rd (3)  ] [  Ra (3)  ] [  000 (3)  ] [ func (3) ]
```

| func[2:0] | Mnemonic | Operation | Privilege |
|-----------|----------|-----------|-----------|
| 000 | GETSP | Rd = SP | — |
| 001 | SETSP | SP = Ra | — |
| 010 | GETPC | Rd = PC + 2 | — |
| 011 | GETFL | Rd = FL | — |
| 100 | SETFL | FL = Ra | privileged |
| 101–111 | — | reserved | — |

SETFL is the only SPREG instruction that requires kernel mode. For GETSP/GETPC/GETFL, only Rd is relevant; Ra is ignored. For SETSP/SETFL, only Ra is relevant; Rd is ignored.

---

## 0x9 — JMP (Format J, 16-bit)

```
 15  14  13  12 | 11  10   9   8   7   6   5   4   3   2   1   0
[  0x9  (4)   ] [                  offset12 (12)                 ]
```

PC = PC + 2 + sign_extend(offset12) × 2

`offset12` is a 12-bit signed word offset relative to the instruction following JMP. Range: **−2048..+2047 words (−4096..+4094 bytes)**.

Does not affect flags.

---

## 0xA — JMPX (Format R, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3 |  2   1   0
[  0xA  (4)   ] [  000 (3)  ] [  Ra (3)  ] [  000 (3)  ] [ func (3) ]
```

| func[2:0] | Mnemonic | Operation | Notes |
|-----------|----------|-----------|-------|
| 000 | JMP Ra | PC = Ra | — |
| 001 | CALL Ra | push (PC+2); PC = Ra | — |
| 010 | RET | PC = mem[SP]; SP += 2 | — |
| 011 | IRET | FL = mem[SP]; SP += 2; PC = mem[SP]; SP += 2 | privileged |
| 100–111 | — | reserved | — |

For JMP/CALL: Ra = target. Rd and Rb positions are encoded as R0 (000).
For RET/IRET: all register fields are R0 (unused).

Does not affect flags (IRET restores FL from stack, replacing all flags).

---

## 0xB — JMPL (Format L, 32-bit)

```
Word 1:  15  14  13  12 | 11  10   9 |  8   7   6 |  5  ..  1 |  0
        [  0xB  (4)   ] [  000 (3)  ] [  000 (3)  ] [ 00000 (5)][ c ]
Word 2: [                          addr16 (16)                       ]
```

- `c` (bit 0) — 0 = JMP, 1 = CALL
- Bits 5–1 reserved, must be 0
- All register fields encode R0 (unused)
- Word 2 — 16-bit absolute target address

| c | Mnemonic | Operation |
|---|----------|-----------|
| 0 | JMP #addr16 | PC = addr16 |
| 1 | CALL #addr16 | push (PC+4); PC = addr16 |

CALL pushes PC+4 (the address of the instruction after the 32-bit CALL). Does not affect flags.

---

## 0xC — BCC (Format B, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6   5   4   3   2   1   0
[  0xC  (4)   ] [ cond (3)  ] [               offset9 (9)        ]
```

If condition holds: PC = PC + 2 + sign_extend(offset9) × 2

`offset9` is a 9-bit signed word offset. Range: **−256..+255 words (−512..+510 bytes)**.

| cond[2:0] | Mnemonic | Condition | Flags tested |
|-----------|----------|-----------|--------------|
| 000 | BEQ | Equal / zero | Z = 1 |
| 001 | BNE | Not equal | Z = 0 |
| 010 | BLT | Less than (signed) | N ≠ V |
| 011 | BGE | Greater or equal (signed) | N = V |
| 100 | BLO | Lower / below (unsigned) | C = 0 |
| 101 | BHS | Higher or same (unsigned) | C = 1 |
| 110 | BMI | Minus / negative | N = 1 |
| 111 | BPL | Plus / positive or zero | N = 0 |

Does not affect flags.

---

## 0xD — SYS (Format S, 16-bit)

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3 |  2   1   0
[  0xD  (4)   ] [  000 (3)  ] [ param (3) ] [  000 (3)  ] [ func (3) ]
```

- `func[2:0]` — selects the system operation
- `param[2:0]` — operation-specific parameter (currently used by TRAP only)
- All register fields encode as R0 (unused)

| func[2:0] | Mnemonic | Operation | param | Privilege |
|-----------|----------|-----------|-------|-----------|
| 000 | HALT | Stop execution | — | privileged |
| 001 | STI | FL.I = 1 (enable interrupts) | — | privileged |
| 010 | CLI | FL.I = 0 (disable interrupts) | — | privileged |
| 011 | TRAP #n | Software interrupt; dispatch to vector 3 | n (0–7) | user-callable |
| 100–111 | — | reserved | — | — |

---

## Pseudo-instruction expansion reference

| Pseudo | Expands to | Encoding |
|--------|------------|---------|
| `NOP` | `ADDI R0, R0, #0` | 0x1, Rd=R0, Ra=R0, imm6=0 |
| `MOV Rd, Rs` | `ADDI Rd, Rs, #0` | 0x1, imm6=0 |
| `MOV Rd, #imm6` | `ADDI Rd, R0, #imm6` | 0x1, Ra=R0 |
| `MOV Rd, #imm16` | `ALUL ADD Rd, R0, #imm16` | 0x4, op=ADD, Ra=R0 |
| `CLR Rd` | `ADDI Rd, R0, #0` | 0x1, Ra=R0, imm6=0 |
| `INC Rd` | `ADDI Rd, Rd, #1` | 0x1, imm6=1 |
| `DEC Rd` | `ADDI Rd, Rd, #-1` | 0x1, imm6=-1 (0x3F) |
| `NOT Rd, Ra` | `ALUL XOR Rd, Ra, #0xFFFF` | 0x4, op=XOR |
| `NEG Rd, Ra` | `ALU SUB Rd, R0, Ra` | 0x0, func=SUB, Ra=R0 |
| `CMP Ra, Rb` | `ALU CMP Ra, Rb` | 0x0, func=CMP, Rd=R0 |
| `CMP Ra, #imm16` | `ALUL CMP Ra, #imm16` | 0x4, op=CMP, Rd=R0 |
| `BRZ Rx, label` | `BEQ Rx, R0, label` | — (see control-flow.md) |
| `BNZ Rx, label` | `BNE Rx, R0, label` | — |
| `JMP label` | `JMP #offset12` or `JMP #addr16` | assembler picks shortest |
| `CALL label` | `CALL Rs` or `CALL #addr16` | assembler picks |

---

## Encoding quick reference

```
Format R:  opcode(4) | Rd(3)  | Ra(3) | Rb(3)     | func(3)
Format I:  opcode(4) | Rd(3)  | Ra(3) | imm6(6)
Format L:  opcode(4) | Rd(3)  | Ra(3) | mode(6)   | + word2(16)
Format J:  opcode(4) | offset12(12)
Format B:  opcode(4) | cond(3)| offset9(9)
Format M:  opcode(4) | Rd(3)  | Ra(3) | W(1) | X(5)  [+ word2(16) if W=0]
Format S:  opcode(4) | 000(3) | param(3) | 000(3) | func(3)
```
