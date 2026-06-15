# Memory instructions

LOAD, STORE, MOV, PUSH, POP.

---

## LOAD

Reads a 16-bit value from memory into a destination register.

```
LOAD  Rd, [Ra]            ; register indirect
LOAD  Rd, [Ra + Rb]       ; register + register
LOAD  Rd, [Ra + #imm6]    ; register + small offset  (±32 bytes, Format I)
LOAD  Rd, [Ra + #imm16]   ; register + large offset  (Format L)
LOAD  Rd, [#addr16]       ; absolute address          (Format L; sugar for [R0 + #addr16])
LOAD  Rd, [PC + #offset]  ; PC-relative               (Format L)
```

LOAD does not affect flags.

`[Ra + #0]` and `[Ra]` assemble identically. The assembler selects the most compact encoding:

| Case | Format | Width |
|------|--------|-------|
| `[Ra]` or `[Ra + #0]` | derived from I with imm6=0 | 16-bit |
| `[Ra + Rb]` | R | 16-bit |
| `[Ra + #imm6]` — offset in −32..+31 | I | 16-bit |
| `[Ra + #imm16]` or `[#addr16]` | L | 32-bit |
| `[PC + #offset]` | L | 32-bit |

### Word alignment

All memory accesses are 16-bit. Addresses must be 2-byte aligned (bit 0 = 0). An unaligned load is undefined behavior; the emulator will trap in debug mode.

### Use as struct field access

`[Ra + #imm6]` is the standard mode for accessing struct fields and stack frame locals. With BP (R6) as the frame pointer:

```asm
LOAD  R1, [BP + #4]    ; load local variable at frame offset 4
LOAD  R1, [BP + #-2]   ; load argument at frame offset -2
```

### Use as array indexing

`[Ra + Rb]` enables computed array access where Rb holds a byte offset:

```asm
LSL   R3, R2, #1       ; R3 = index * 2  (for 16-bit element array)
LOAD  R1, [R4 + R3]    ; load array[index]
```

### Position-independent data access

`[PC + #offset]` lets code reference nearby data without knowing its absolute address:

```asm
LOAD  R1, [PC + #6]    ; load the word 6 bytes ahead
```

---

## STORE

Writes a 16-bit register value to memory.

```
STORE  [Ra], Rs            ; register indirect
STORE  [Ra + Rb], Rs       ; register + register
STORE  [Ra + #imm6], Rs    ; register + small offset
STORE  [Ra + #imm16], Rs   ; register + large offset
STORE  [#addr16], Rs       ; absolute address
```

STORE does not affect flags. There is no PC-relative STORE.

In the instruction encoding, the field that holds `Rd` for other instructions holds `Rs` (the source register) for STORE. The assembler handles this transparently.

Writes to ROM (0x8000–0xBFFF) are silently ignored. Writes to the I/O region (0xFF00–0xFFFF) update device registers.

---

## MOV

Copy a register or load a constant. All MOV forms are assembler pseudo-instructions that expand to ADD.

```
MOV  Rd, Rs            ; copy register       → ADD Rd, Rs, R0
MOV  Rd, #imm6         ; load small constant → ADD Rd, R0, #imm6
MOV  Rd, #imm16        ; load full constant  → ADD Rd, R0, #imm16
```

Because MOV expands to ADD, it **does** affect flags. In flag-sensitive code, use the underlying ADD directly if you need to document intent, or reload flags afterward.

### Special register moves

LOAD and STORE cannot access SP, PC, or FL directly. Use these dedicated instructions instead:

```
GETSP  Rd              ; Rd = SP
SETSP  Rs              ; SP = Rs
GETPC  Rd              ; Rd = address of instruction following GETPC
GETFL  Rd              ; Rd = FL (full flags register as a 16-bit value)
SETFL  Rs              ; FL = Rs   (privileged — traps in user mode)
```

None of these affect flags (SETFL is the exception — it replaces FL entirely, so all flags change).

---

## PUSH

Pushes a 16-bit register value onto the stack. Full descending convention: SP is decremented first, then the value is written.

```
PUSH  Rs               ; SP -= 2; mem[SP] = Rs
```

PUSH does not affect flags.

PUSH is not a pseudo-instruction. The decrement-then-write must be atomic from the perspective of interrupt delivery — the CPU will not take an interrupt between the SP update and the write.

---

## POP

Pops a 16-bit value from the stack into a register. The value is read, then SP is incremented.

```
POP   Rd               ; Rd = mem[SP]; SP += 2
```

POP does not affect flags.

`POP R0` is valid: it discards the value and adjusts SP by 2. Useful for removing a value that was pushed but no longer needed (e.g., after a CALL that passes an argument on the stack).

---

## Byte access

All memory instructions operate on 16-bit words. There are no byte-granularity LOAD or STORE instructions.

To read a single byte from address `addr`:

```asm
; assume addr is byte address; the containing word is at (addr & 0xFFFE)
LOAD  R1, [Ra]         ; load the 16-bit word containing the target byte
; if addr is even (low byte):
AND   R1, R1, #0x00FF
; if addr is odd (high byte):
LSR   R1, R1, #8
```

To write a single byte, read-modify-write:

```asm
LOAD  R2, [Ra]         ; read containing word
; modify low byte of R2:
AND   R2, R2, #0xFF00  ; clear low byte
OR    R2, R2, R3       ; insert new byte (R3 must have byte in low position)
STORE [Ra], R2         ; write back
```

This is a known limitation of the v1 ISA. A byte LOAD/STORE extension is a candidate for a future revision.
