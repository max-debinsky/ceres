# Control flow instructions

JMP, CALL, RET, IRET, conditional branches, and system instructions.

---

## JMP

Unconditional jump. Transfers control immediately; the instruction after JMP is not executed.

```
JMP   Rs               ; PC = Rs                        (register, 16-bit)
JMP   #offset12        ; PC = PC + 2 + offset12 × 2    (PC-relative short, 16-bit)
JMP   #addr16          ; PC = addr16                    (absolute, 32-bit)
```

JMP does not affect flags.

`offset12` is a 12-bit signed value (−2048..+2047), counted in **words** (2 bytes each). The offset is measured from the instruction immediately following the JMP. This gives a ±4 KB reach from the JMP instruction, covering the majority of loops and local branches. For targets beyond this range, use the absolute form.

`JMP Rs` is used for computed jumps (switch tables, function pointers, indirect calls).

---

## CALL

Pushes the return address onto the stack and jumps to the target.

```
CALL  Rs               ; push (PC + 2); PC = Rs         (register, 16-bit)
CALL  #addr16          ; push (PC + 4); PC = addr16     (absolute, 32-bit)
```

The pushed value is the address of the instruction that follows the CALL — the natural resume point after RET.

CALL does not affect flags (other than the SP change).

```asm
; standard subroutine call
CALL  draw_sprite
; execution resumes here after RET
```

---

## RET

Returns from a subroutine by popping the return address from the stack into PC.

```
RET                    ; PC = mem[SP]; SP += 2
```

RET does not affect flags. It is equivalent to `POP PC` in effect, but has its own encoding so the assembler can distinguish it from a general-purpose POP.

---

## IRET

Returns from an interrupt handler. Pops FL then PC from the stack, restoring the privilege level and interrupt-enable state that were active before the interrupt.

```
IRET                   ; FL = mem[SP]; SP += 2; PC = mem[SP]; SP += 2
```

IRET is privileged — it traps in user mode. See [`../architecture/interrupts.md`](../architecture/interrupts.md) for the full interrupt entry/exit sequence.

The order of pops (FL first, then PC) means that the interrupt handler's stack frame must have been built with PC pushed first and FL pushed second (which is what the hardware interrupt entry does).

---

## Conditional branches

All conditional branches are PC-relative. The 9-bit signed offset (−256..+255) is counted in words from the instruction following the branch, giving a ±512-byte reach.

```
B<cond>  label         ; if condition holds: PC = PC + 2 + offset9 × 2
```

If the condition does not hold, execution falls through to the next instruction.

### Condition codes

| Mnemonic | Meaning | Flags tested | Interpretation |
|----------|---------|--------------|----------------|
| BEQ | Equal / zero | Z = 1 | signed or unsigned |
| BNE | Not equal / non-zero | Z = 0 | signed or unsigned |
| BLT | Less than | N ≠ V | **signed** |
| BGE | Greater than or equal | N = V | **signed** |
| BLO | Lower (below) | C = 0 | **unsigned** |
| BHS | Higher or same | C = 1 | **unsigned** |
| BMI | Minus / negative | N = 1 | — |
| BPL | Plus / positive or zero | N = 0 | — |

These are the same 8 condition codes from `cpu.md` (cond field in Format B).

### Signed vs unsigned: which to use

Use CMP to set flags, then pick a branch based on whether the values are signed or unsigned:

```asm
; Signed comparison: is R1 < R2?
CMP   R1, R2
BLT   signed_less

; Unsigned comparison: is R1 < R2?
CMP   R1, R2
BLO   unsigned_less
```

BEQ and BNE are safe for either type — they only test Z.

### Greater-than and less-than-or-equal

BGT (greater-than) and BLE (less-than-or-equal) are not in the condition table, but are easy to synthesize by reversing the operands in CMP:

```asm
; Branch if R1 > R2 (signed)
CMP   R2, R1           ; reverse: now test R2 < R1
BLT   greater_than

; Branch if R1 <= R2 (signed)
CMP   R2, R1
BGE   less_or_equal
```

Equivalently, negate the condition and use a fall-through:

```asm
CMP   R1, R2
BGE   not_less         ; skip body if R1 >= R2
; body runs when R1 < R2
not_less:
```

### Long branches

For branch targets beyond ±512 bytes, use an inverted branch over a JMP:

```asm
BEQ   skip
JMP   #far_target
skip:
```

---

## System instructions

### HALT

Stops the CPU. The emulator pauses. On hardware, the CPU enters a low-power stopped state and waits for an interrupt or reset.

```
HALT                   ; stop execution (privileged)
```

Privileged — traps in user mode.

### STI and CLI

Control the interrupt enable flag (I bit in FL).

```
STI                    ; FL.I = 1  (enable maskable interrupts, privileged)
CLI                    ; FL.I = 0  (disable maskable interrupts, privileged)
```

Both are privileged. On reset, I = 0. The boot sequence calls STI after initialization.

### TRAP

Software interrupt. Used by user-mode code to request kernel services. Transfers control to the trap vector (vector 3) in kernel mode.

```
TRAP  #n               ; raise software interrupt n   (n = 0..7, user-mode callable)
```

TRAP is not privileged — it is the sanctioned path from user mode into the kernel. The 3-bit n value is encoded in the instruction word; by convention, n identifies the service class, and further arguments are passed in registers.

```asm
; User-mode kernel call convention (example)
MOV   R4, #1           ; file descriptor
MOV   R5, buffer       ; pointer
MOV   R6, #256         ; count
TRAP  #0               ; syscall
; return value in R1
```
