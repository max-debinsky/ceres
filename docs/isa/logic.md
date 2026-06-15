# Logic and shift instructions

AND, OR, XOR, NOT. Shifts: LSL, LSR, ASR, ROR. No dedicated bit-set or bit-clear instructions — use AND and OR with masks.

---

## AND

Bitwise AND. Commonly used to mask off (clear) bits.

```
AND  Rd, Ra, Rb        ; Rd = Ra & Rb
AND  Rd, Ra, #imm6     ; Rd = Ra & zero_extend(imm6)
AND  Rd, Ra, #imm16    ; Rd = Ra & imm16
```

Flags: Z, N. C and V are cleared to 0.

Unlike arithmetic instructions, the immediate for AND, OR, and XOR is **zero-extended** (not sign-extended). A mask of `#0x3F` (6 bits, all ones) works directly in Format I. Masks wider than 6 bits require Format L:

```asm
AND  R1, R1, #0x001F   ; keep low 5 bits (Format I, imm6=31)
AND  R1, R1, #0x00FF   ; keep low byte   (Format L, imm16=255)
AND  R1, R1, #0x0F0F   ; nibble mask     (Format L)
```

`AND R0, Ra, #mask` tests bits without storing the result — only the flags are written (Z=1 if no bits matched).

---

## OR

Bitwise OR. Used to set specific bits.

```
OR   Rd, Ra, Rb        ; Rd = Ra | Rb
OR   Rd, Ra, #imm6     ; Rd = Ra | zero_extend(imm6)
OR   Rd, Ra, #imm16    ; Rd = Ra | imm16
```

Flags: Z, N. C and V are cleared to 0.

```asm
OR   R1, R1, #0x0001   ; set bit 0         (Format I)
OR   R1, R1, #0x8000   ; set bit 15        (Format L)
OR   R1, R2, R3        ; combine two values (Format R)
```

---

## XOR

Bitwise XOR. Used to toggle bits or check equality.

```
XOR  Rd, Ra, Rb        ; Rd = Ra ^ Rb
XOR  Rd, Ra, #imm6     ; Rd = Ra ^ zero_extend(imm6)
XOR  Rd, Ra, #imm16    ; Rd = Ra ^ imm16
```

Flags: Z, N. C and V are cleared to 0.

`XOR Ra, Ra, Ra` sets Ra to zero and sets Z=1 (identical to `MOV Ra, R0` but does affect flags).

---

## NOT

Bitwise complement. Expands to `XOR Rd, Ra, #0xFFFF` — there is no separate NOT opcode.

```
NOT  Rd, Ra            ; pseudo: XOR Rd, Ra, #0xFFFF
```

Flags: Z, N (same as XOR).

---

## Bit manipulation without dedicated instructions

There are no BIT, SET, or CLR instructions. The patterns using AND, OR, and XOR are standard:

```asm
; Test bit n: AND result to R0 (discarded), Z=1 if bit was clear
AND   R0, R1, #(1 << n)

; Set bit n
OR    R1, R1, #(1 << n)

; Clear bit n
AND   R1, R1, #~(1 << n)

; Toggle bit n
XOR   R1, R1, #(1 << n)
```

The assembler evaluates constant expressions like `(1 << n)` and `~(1 << n)` at assembly time. Masks for bits 0–5 fit in imm6 (Format I, 16-bit instruction); bits 6–15 require Format L (32-bit).

---

## LSL — logical shift left

Shifts Ra left by n positions. Zero bits enter from the right. For unsigned values this is multiplication by 2ⁿ (without overflow detection).

```
LSL  Rd, Ra, Rb        ; Rd = Ra << Rb[3:0]   (shift amount from low 4 bits of Rb)
LSL  Rd, Ra, #n        ; Rd = Ra << n          (n = 0..15)
```

Flags: Z, N, C (last bit shifted out of bit 15). V is undefined.

---

## LSR — logical shift right

Shifts Ra right by n positions. Zero bits enter from the left. For unsigned values this is division by 2ⁿ (rounding toward zero).

```
LSR  Rd, Ra, Rb        ; Rd = Ra >> Rb[3:0]   (unsigned, zero fill)
LSR  Rd, Ra, #n        ; Rd = Ra >> n
```

Flags: Z, N (always 0 after LSR), C (last bit shifted out of bit 0). V is undefined.

---

## ASR — arithmetic shift right

Shifts Ra right by n positions. The sign bit (bit 15) is replicated into vacated positions. For signed values this is division by 2ⁿ rounding toward −∞ (floor division).

```
ASR  Rd, Ra, Rb        ; Rd = Ra >> Rb[3:0]   (signed, sign fill)
ASR  Rd, Ra, #n        ; Rd = Ra >> n
```

Flags: Z, N, C (last bit shifted out). V is undefined.

ASR by 1 is the standard signed halving operation. `ASR Rd, Ra, #15` fills Rd with 0x0000 if Ra ≥ 0, or 0xFFFF if Ra < 0 — useful for computing sign masks.

---

## ROR — rotate right

Rotates Ra right by n positions. Bits shifted out of bit 0 reenter at bit 15.

```
ROR  Rd, Ra, Rb        ; Rd = rotate_right(Ra, Rb[3:0])
ROR  Rd, Ra, #n        ; Rd = rotate_right(Ra, n)
```

Flags: Z, N, C (the bit that entered bit 15 also sets C). V is undefined.

There is no ROL instruction. Rotate left by n = rotate right by (16 − n):

```asm
ROR  R1, R1, #12       ; equivalent to ROL R1, R1, #4
```

---

## Shift amount encoding

The shift amount is a 4-bit unsigned value (0–15). In Format R, the low 4 bits of Rb supply the amount at runtime; the upper 12 bits of Rb are ignored. In the immediate form, the assembler encodes n directly. A shift amount of 0 is valid and produces no change (flags still update).

The four shift types (LSL, LSR, ASR, ROR) are distinguished by a 2-bit sub-type field within the shift instruction encoding. The assembler selects the correct sub-type from the mnemonic.
