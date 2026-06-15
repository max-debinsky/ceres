# Arithmetic instructions

ADD, SUB, MUL, CMP. No hardware DIV — use the ROM library routines.

---

## ADD

```
ADD  Rd, Ra, Rb        ; Rd = Ra + Rb
ADD  Rd, Ra, #imm6     ; Rd = Ra + sign_extend(imm6)
ADD  Rd, Ra, #imm16    ; Rd = Ra + imm16
```

Flags: Z, N, C, V.

- **C** is set if the addition produces a carry out of bit 15 (unsigned overflow).
- **V** is set if the result's sign is wrong for the operands (signed overflow: two positive numbers give a negative result, or vice versa).

`ADD Rd, R0, #imm` is the canonical way to load a small constant into a register (the assembler pseudo `MOV Rd, #imm` expands to this).

---

## SUB

```
SUB  Rd, Ra, Rb        ; Rd = Ra - Rb
SUB  Rd, Ra, #imm6     ; Rd = Ra - sign_extend(imm6)
SUB  Rd, Ra, #imm16    ; Rd = Ra - imm16
```

Flags: Z, N, C, V.

- **C** is set if a borrow was required (Ra < Rb unsigned). This is the inverse of ADD — C=0 after SUB means borrow occurred, C=1 means no borrow.
- **V** is set on signed overflow.

`SUB Rd, R0, Ra` negates Ra (result = 0 − Ra = −Ra in two's complement).

---

## MUL

Hardware 16×16→16 multiply. The result is the low 16 bits of the full 32-bit product. The upper 16 bits are silently discarded.

```
MUL  Rd, Ra, Rb        ; Rd = (Ra × Rb)[15:0]
MUL  Rd, Ra, #imm6     ; Rd = (Ra × sign_extend(imm6))[15:0]
```

Flags: Z, N. C and V are **undefined** after MUL.

The low 16 bits of a 16×16 multiply are identical for signed and unsigned operands, so one MUL instruction covers both. If you need the full 32-bit product, use the ROM widening multiply:

```asm
; In:  R4 = multiplicand, R5 = multiplier
; Out: R1 = high 16 bits, R2 = low 16 bits
CALL  MUL_U32          ; unsigned widening multiply
CALL  MUL_S32          ; signed widening multiply
```

---

## CMP

Compares two values by computing Ra − Rb and discarding the result. Sets flags exactly as SUB does.

```
CMP  Ra, Rb            ; flags from Ra - Rb   (Rd field = R0, result discarded)
CMP  Ra, #imm6         ; flags from Ra - imm6
CMP  Ra, #imm16        ; flags from Ra - imm16
```

Flags: Z, N, C, V.

CMP is the standard setup for a conditional branch:

```asm
CMP   R1, #100
BLT   loop_body        ; branch if R1 < 100 (signed)

CMP   R2, R3
BHS   skip             ; branch if R2 >= R3 (unsigned)
```

CMP is not a separate opcode — it is SUB with Rd = R0. The result is discarded because any write to R0 is ignored.

---

## Division (software only)

There is no hardware DIV instruction. Division is provided as ROM library routines.

```asm
; In:  R4 = dividend, R5 = divisor
; Out: R1 = quotient,  R2 = remainder
CALL  DIV_S16          ; signed 16-bit divide
CALL  DIV_U16          ; unsigned 16-bit divide
```

On divide-by-zero: R1 = 0xFFFF, R2 = 0 (unsigned); R1 = 0x7FFF or 0x8000 depending on sign (signed). No trap is raised.

---

## Encoding summary

ADD, SUB, MUL, and CMP share the ALU opcode family. The instruction format is chosen by the assembler based on the operand:

| Form | Format | When used |
|------|--------|-----------|
| `OP Rd, Ra, Rb` | R (16-bit) | Both sources are registers |
| `OP Rd, Ra, #imm6` | I (16-bit) | Constant fits in −32..+31 |
| `OP Rd, Ra, #imm16` | L (32-bit) | Constant needs full 16-bit range |

The assembler selects the shortest valid encoding automatically.
