# Architecture overview

CPU-16 is a fictional 16-bit home computer designed from scratch. This document describes the machine at the highest level and links to detailed documentation for each subsystem.

---

## Top-level block diagram

```
┌─────────────────────────────────────────────┐
│                  CPU core                   │
│  ┌─────────────┐  ┌────────┐                │
│  │  Registers  │  │  ALU   │                │
│  │  R0–R7      │  │        │                │
│  │  PC SP FL   │  │Z N C V │                │
│  └─────────────┘  └────────┘                │
│  ┌──────────────────────────────────────┐   │
│  │  Instruction decoder (16/32-bit)     │   │
│  └──────────────────────────────────────┘   │
│  ┌──────────────────────────────────────┐   │
│  │  Interrupt controller (I, P flags)   │   │
│  └──────────────────────────────────────┘   │
└───────────────────┬─────────────────────────┘
                    │ 16-bit address + data bus
       ┌────────────┼────────────┬─────────────────┐
       │            │            │                 │
  ┌────┴────┐  ┌────┴────┐  ┌───┴────┐  ┌─────────┴──────┐
  │ 64 KB   │  │  ROM    │  │  I/O   │  │ Video          │
  │  RAM    │  │ (BASIC) │  │  regs  │  │ controller     │
  └─────────┘  └─────────┘  └───┬────┘  └────────┬───────┘
                                 │                │ private bus
                            ┌────┴────┐      ┌────┴───────┐
                            │Keyboard │      │  64 KB     │
                            │ timer   │      │  VRAM      │
                            └─────────┘      └────────────┘
                                                  │
                                             ┌────┴───────┐
                                             │  Display   │
                                             │ 320×200    │
                                             └────────────┘
```

---

## Subsystems

### CPU

A single-core 16-bit processor. Eight general-purpose 16-bit registers (R0 hardwired to zero), a program counter, stack pointer, and flags register. Variable-length instruction encoding: 16-bit for register operations and short immediates, 32-bit for full 16-bit immediates. Seven addressing modes.

→ See [`cpu.md`](cpu.md)

### Memory

64 KB flat address space. No banking required for basic use; the architecture is designed so banking can be added later as an extension. ROM lives in the upper half of the address space. I/O registers are memory-mapped at the top of the address space.

→ See [`memory-map.md`](memory-map.md)

### Graphics

320×200 pixels, 256-color palette, pure framebuffer. One byte per pixel. The framebuffer lives in a dedicated 64 KB VRAM chip that is completely outside the CPU's address space. The CPU writes pixels by sending commands through memory-mapped video controller registers. The palette is 256 entries of 24-bit RGB.

→ See [`graphics.md`](graphics.md)

### Interrupts

A priority-based interrupt system. The I flag in FL enables and disables maskable interrupts globally. The P flag implements a two-level privilege model (user/kernel) to support optional OS use. Interrupt vectors live in the zero page.

→ See [`interrupts.md`](interrupts.md)

### I/O

Memory-mapped I/O at `0xFF00–0xFFFF`. Keyboard, timers, and video controller registers all live here. The CPU communicates with VRAM entirely through these registers — there are no special I/O instructions; ordinary LOAD and STORE to the I/O region do the work.

---

## Key design decisions (summary)

Each of these is documented in full detail in the relevant file. This table is a quick reference for the rationale behind major choices.

| Decision | Choice | Why |
|---|---|---|
| Word size | 16-bit | Home computer sweet spot — enough for real programs, simple to understand |
| Register count | 8 GP registers | Compiler-friendly without wasting encoding bits; fits in 3 bits |
| R0 behavior | Hardwired zero | Eliminates CLR, simplifies branch-to-zero patterns, costs nothing |
| Instruction length | Variable 16/32-bit | Dense common case, full immediates without tricks |
| Flags | Z, N, C, V, I, P | Complete signed and unsigned arithmetic; future OS support |
| Stack convention | Full descending | Standard; consistent with ARM, x86, 68000 |
| VRAM location | Outside CPU space | Full 64 KB for programs; framebuffer does not consume address space |
| Graphics model | Framebuffer only | Maximum simplicity; programmer has full pixel control |
| Color depth | 8bpp, 256-color palette | One byte per pixel — trivial address calculation |
| Addressing modes | All 7 | Covers every real use case; struct access, array indexing, position independence |

---

## What is deliberately absent

Some things were left out on purpose:

- **Hardware sprites and tiles.** The framebuffer model is simpler and more general. Sprites can be implemented in software.
- **DMA.** A future extension candidate, but adds significant complexity. Excluded from v1.
- **Floating point.** Out of scope. A software floating-point library is the intended path.
- **Memory protection / MMU.** The P flag supports a basic kernel/user distinction. Full memory protection is a future extension.
