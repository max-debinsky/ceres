# CPU

The CPU-16 processor is a single-core 16-bit CPU. This document specifies its registers, flags, instruction encoding formats, addressing modes, and stack behavior.

---

## Registers

### General-purpose registers

There are eight 16-bit general-purpose registers, named R0 through R7.

| Register | Assembler alias | Convention | Notes |
|---|---|---|---|
| R0 | ZERO | — | **Hardwired zero.** Reads always return 0. Writes are silently discarded. |
| R1 | — | Return value | Holds function return values by convention |
| R2 | — | Scratch | Caller-saved scratch register |
| R3 | — | Scratch | Caller-saved scratch register |
| R4 | — | Argument 1 | First function argument by convention |
| R5 | — | Argument 2 | Second function argument by convention |
| R6 | BP | Base pointer | Frame pointer for stack frames; assembler accepts `BP` as an alias for `R6` |
| R7 | — | Scratch | Caller-saved scratch register |

The conventions above (return value, scratch, argument, base pointer) are **software conventions only**. The hardware does not enforce them. R6 is physically identical to any other general-purpose register; the `BP` alias exists in the assembler for readability.

**R0 hardwired zero** is one of the most useful properties of the register file. Because any write to R0 is discarded and any read returns zero, many common operations can be expressed without dedicated instructions:

```asm
MOV  R1, R0          ; clear R1 to zero  (no CLR instruction needed)
SUB  R1, R1, R0      ; no-op             (useful NOP alias)
BEQ  R1, R0, label   ; branch if R1 == 0 (no dedicated BRZ instruction needed)
ADD  R1, R0, #42     ; load small immediate into R1
```

### Special registers

| Register | Width | Description |
|---|---|---|
| PC | 16-bit | Program counter. Points to the next instruction to fetch. |
| SP | 16-bit | Stack pointer. See stack behavior below. |
| FL | 16-bit | Flags register. |

PC and SP can be read and written by specific instructions. FL can be read with a dedicated instruction and individual bits are set/cleared by the ALU and control instructions.

---

## Flags register (FL)

```
Bit:  15  14  13  12  11  10  9   8   7   6   5   4   3   2   1   0
      ─── ─── ─── ─── ─── ─── ─── ─── ─── ─── [P] [I] [V] [C] [N] [Z]
      reserved (read as zero, writes ignored)
```

| Bit | Name | Set when |
|---|---|---|
| 0 | **Z** Zero | Result of the last ALU operation was exactly zero |
| 1 | **N** Negative | Bit 15 (MSB) of the result was 1 (two's complement negative) |
| 2 | **C** Carry | Addition produced a carry out of bit 15, or subtraction required a borrow |
| 3 | **V** Overflow | Signed arithmetic overflow: the result's sign is wrong for the operands |
| 4 | **I** Interrupt enable | When 1, maskable hardware interrupts are accepted. When 0, they are held pending. |
| 5 | **P** Privilege | 0 = user mode. 1 = kernel mode. Privileged instructions trap in user mode. |
| 6–15 | reserved | Always read as zero. Writes are ignored. |

### Z and N

Z and N are straightforward and set after every ALU operation (ADD, SUB, AND, OR, XOR, NOT, shifts). They are not affected by LOAD, STORE, or control-flow instructions.

### C and V — the difference

C and V are both about overflow, but for different number interpretations:

- **C (carry)** reflects unsigned overflow. `0xFFFF + 0x0001` produces `0x0000` with C=1. Used for multi-word arithmetic and unsigned comparisons.
- **V (overflow)** reflects signed overflow. Adding two positive numbers and getting a negative result sets V=1. `0x7FFF + 0x0001` produces `0x8000` (looks negative) with V=1 but C=0.

Both are needed for a complete arithmetic system. Without V, signed comparisons are unreliable. Without C, multi-word (32-bit+) arithmetic is impossible.

### I — interrupt enable

Setting I=1 allows the interrupt controller to deliver maskable interrupts to the CPU. When I=0, interrupts are held pending and delivered as soon as I is set to 1 again. Non-maskable interrupts (NMI), if implemented, bypass this flag.

Instructions that modify I are privileged (require P=1).

### P — privilege mode

P=0 is user mode. P=1 is kernel mode. In user mode, certain instructions (halt, direct FL write, I/O to protected ports) cause a privilege trap, transferring control to the kernel's trap handler.

P can only be set to 1 by the hardware (on reset, or when an interrupt/trap fires). Software can lower privilege by switching to user mode, but cannot raise it directly — only an interrupt or trap can do that.

---

## Instruction encoding

Instructions are **variable length**: either 16 bits (one word) or 32 bits (two words). The decoder reads the first word and determines whether a second word follows.

There are five instruction formats.

### Format R — register operation (16-bit)

Used for operations that take two source registers and one destination register.

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3 |  2   1   0
[  opcode (4)  ] [  Rd (3)  ] [  Ra (3)  ] [  Rb (3)  ] [ func (3) ]
```

- `opcode` — identifies the instruction family (e.g. arithmetic)
- `Rd` — destination register
- `Ra` — first source register
- `Rb` — second source register
- `func` — sub-function within the family (e.g. ADD vs SUB)

Example: `ADD R1, R2, R3` encodes Rd=R1, Ra=R2, Rb=R3, func=ADD.

### Format I — small immediate (16-bit)

Used for operations with one register source and a small constant.

```
 15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3   2   1   0
[  opcode (4)  ] [  Rd (3)  ] [  Ra (3)  ] [       imm6 (6)       ]
```

- `imm6` — a 6-bit signed immediate, range −32 to +31

Example: `ADD R1, R2, #5` encodes Rd=R1, Ra=R2, imm6=5.

The small range of imm6 is a deliberate tradeoff against code density. For larger constants, use Format L.

### Format L — large immediate (32-bit)

Used when a full 16-bit constant is needed. The second word carries the value.

```
Word 1:  15  14  13  12 | 11  10   9 |  8   7   6 |  5   4   3   2   1   0
        [  opcode (4)  ] [  Rd (3)  ] [  Ra (3)  ] [      mode (6)       ]
Word 2: [                      imm16 (16)                                  ]
```

- `mode` — encodes the addressing mode and any additional options
- `imm16` — full 16-bit value (signed or unsigned depending on instruction)

Example: `MOV R1, #1000` requires Format L because 1000 > 31.

The decoder identifies a Format L instruction when the opcode field and mode bits indicate a large immediate is expected. The exact detection encoding will be specified in the ISA opcode table.

### Format J — jump (16-bit)

Used for unconditional jumps with a PC-relative offset.

```
 15  14  13  12 | 11  10   9   8   7   6   5   4   3   2   1   0
[  opcode (4)  ] [                  offset12 (12)                ]
```

- `offset12` — a 12-bit signed offset relative to the instruction after the jump, range −2048 to +2047 words

For jumps beyond this range, Format L provides a full 16-bit target address.

### Format B — conditional branch (16-bit)

Used for conditional branches with a condition code and PC-relative offset.

```
 15  14  13  12 | 11  10   9 |  8   7   6   5   4   3   2   1   0
[  opcode (4)  ] [  cond (3) ] [              offset9 (9)         ]
```

- `cond` — 3-bit condition code (see branch conditions table in `control-flow.md`)
- `offset9` — 9-bit signed offset, range −256 to +255 words

Condition codes:

| Code | Mnemonic | Meaning | Flags tested |
|---|---|---|---|
| 000 | EQ | Equal / zero | Z=1 |
| 001 | NE | Not equal / not zero | Z=0 |
| 010 | LT | Less than (signed) | N≠V |
| 011 | GE | Greater than or equal (signed) | N=V |
| 100 | LO | Lower (unsigned) | C=0 |
| 101 | HS | Higher or same (unsigned) | C=1 |
| 110 | MI | Minus / negative | N=1 |
| 111 | PL | Plus / positive or zero | N=0 |

---

## Addressing modes

The CPU supports seven addressing modes. Not all modes are available for all instructions — the ISA opcode table specifies which modes each instruction accepts.

| Mode | Assembler syntax | Meaning |
|---|---|---|
| Register | `R1` | The value in register R1 |
| Immediate | `#42` | The literal constant 42 |
| Absolute | `[0x1000]` | The value in memory at address 0x1000 |
| Register indirect | `[R1]` | The value in memory at the address held in R1 |
| Register + offset | `[R1 + 4]` | The value in memory at address (R1 + 4) |
| PC-relative | `[PC + offset]` | The value in memory at address (PC + offset) |
| Register + register | `[R1 + R2]` | The value in memory at address (R1 + R2) |

Register + offset is the workhorse mode for struct field access and stack frame locals (using BP as the base). Register + register enables array indexing when R2 holds a computed index.

PC-relative addressing is always used for branch and jump offsets. It can also be used for data, enabling position-independent code.

---

## Stack behavior

The stack uses the **full descending** convention:

- SP points to the **last pushed item** (the current top of the stack)
- The stack **grows downward** (toward lower addresses)
- On reset, SP is initialized to `0x01FF`

**PUSH Rx** sequence:
1. Decrement SP by 2
2. Write the 16-bit value of Rx to memory at SP

**POP Rx** sequence:
1. Read the 16-bit value at memory address SP into Rx
2. Increment SP by 2

**CALL addr** sequence:
1. Decrement SP by 2
2. Write PC (the return address) to memory at SP
3. Set PC to addr

**RET** sequence:
1. Read the 16-bit value at SP into PC
2. Increment SP by 2

The hardware stack is in main RAM. The stack pointer is a general-purpose 16-bit register — it has no hardware range check. Stack overflow is a software concern.

---

## Cycle timing

*To be specified.* The timing model will define how many clock cycles each instruction takes. The intent is a simple model: most register operations take 1 cycle, memory accesses take 2–3 cycles, multiply/divide take more. An exact table will be produced when the emulator is designed.
