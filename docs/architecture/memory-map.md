# Memory map

The CPU-16 has a **16-bit address bus**, giving it a flat address space of 65,536 bytes (64 KB). This document describes the layout of that space and the design rationale behind it.

VRAM is a separate 64 KB chip that does not occupy any of this space.

---

## Address space map

```
0xFFFF  ┌───────────────────────────────┐
        │  Memory-mapped I/O (256 B)    │  0xFF00–0xFFFF
0xFF00  ├───────────────────────────────┤
        │                               │
        │  Reserved / expansion         │  0xC000–0xFEFF
        │                               │
0xC000  ├───────────────────────────────┤
        │                               │
        │  ROM — BASIC + boot code      │  0x8000–0xBFFF
        │  (16 KB)                      │
0x8000  ├───────────────────────────────┤
        │                               │
        │  RAM — user programs, heap    │  0x0200–0x7FFF
        │  (~31.5 KB)                   │
        │                               │
0x0200  ├───────────────────────────────┤
        │  Hardware stack (256 B)       │  0x0100–0x01FF
0x0100  ├───────────────────────────────┤
        │  Zero page (256 B)            │  0x0000–0x00FF
0x0000  └───────────────────────────────┘
```

---

## Regions

### Zero page — 0x0000–0x00FF (256 bytes)

The first 256 bytes of memory have special status. Interrupt vectors are stored here, making them easy to find and, if needed, easy to patch at runtime.

| Address | Contents |
|---|---|
| 0x0000–0x001F | Interrupt vector table (16 vectors × 2 bytes each) |
| 0x0020–0x00FF | Reserved for system use / future expansion |

Accessing the zero page is not architecturally faster on CPU-16 (unlike the 6502), but the convention of placing vectors and critical system pointers here is maintained for clarity.

### Hardware stack — 0x0100–0x01FF (256 bytes)

By convention, the stack lives here. SP is initialized to 0x01FF on reset, and the first PUSH will write to 0x01FF.

This is a **software convention**, not a hardware constraint. Nothing prevents SP from being changed to point anywhere in RAM. Programs that need a larger stack can move SP upward into the general RAM region.

256 bytes (128 push/pop operations at 16-bit) is enough for modest programs. A program that needs deeper call stacks should explicitly set SP to a higher address early in initialization.

### RAM — 0x0200–0x7FFF (~31.5 KB)

General-purpose RAM available to user programs. On reset, the contents of RAM are undefined. Programs must initialize any state they depend on.

The upper boundary of the RAM region (0x7FFF) places ROM at the natural 32 KB boundary. This keeps the memory map symmetric and makes address decoding straightforward in hardware: if bit 15 of the address is 0, the access goes to RAM. If bit 15 is 1 and the address is below 0xFF00, the access goes to ROM.

### ROM — 0x8000–0xBFFF (16 KB)

Read-only memory. Contains:
- The boot sequence (reset handler)
- Standard library routines (math, string handling, I/O primitives)
- Font data for text rendering

16 KB is a provisional size. The exact ROM size will be determined when the BASIC interpreter is designed. The address range can be adjusted if needed; 16 KB is a reasonable estimate for a compact BASIC with useful built-ins.

Writes to the ROM region are silently ignored by the hardware. In the emulator, writes to this region will log a warning in debug mode.

### Reserved — 0xC000–0xFEFF (~15.7 KB)

Currently unused. Candidates for future use:

- Extended ROM (if BASIC grows beyond 16 KB)
- Banked RAM window (if banking is added later)
- Additional I/O device registers
- Cartridge or expansion port mapping

### Memory-mapped I/O — 0xFF00–0xFFFF (256 bytes)

All hardware registers are mapped here. The CPU communicates with every peripheral — keyboard, timer, video controller, audio — by reading and writing addresses in this region.

Using a dedicated I/O region (rather than special IN/OUT instructions) means that ordinary LOAD and STORE instructions access peripherals. This keeps the ISA simpler and allows I/O access from any addressing mode.

The full I/O register map is specified in the I/O documentation. Preliminary layout:

| Address | Device | Register |
|---|---|---|
| 0xFF00 | Video | Control / command |
| 0xFF01 | Video | VRAM address (low byte) |
| 0xFF02 | Video | VRAM address (high byte) |
| 0xFF03 | Video | VRAM data (read/write one byte) |
| 0xFF04 | Video | Palette index |
| 0xFF05 | Video | Palette data (R) |
| 0xFF06 | Video | Palette data (G) |
| 0xFF07 | Video | Palette data (B) |
| 0xFF10 | Keyboard | Key status / scancode |
| 0xFF11 | Keyboard | Control |
| 0xFF20 | Timer 0 | Counter (low) |
| 0xFF21 | Timer 0 | Counter (high) |
| 0xFF22 | Timer 0 | Control |
| 0xFF30 | Audio | TBD |
| 0xFF40–0xFFEF | — | Reserved |
| 0xFFF0–0xFFFE | System | Reserved |
| 0xFFFF | System | Reset / watchdog |

---

## VRAM (outside CPU address space)

The framebuffer does not live in the 64 KB CPU address space. It is a separate 64 KB chip connected to the video controller. The CPU accesses VRAM indirectly through the video controller registers at 0xFF00–0xFF07.

To write a single pixel:
1. Write the VRAM address to 0xFF01 (low) and 0xFF02 (high)
2. Write the color byte to 0xFF03

The video controller can also support block transfer modes (writing a run of bytes to VRAM without repeated address updates) — this will be specified in the graphics documentation.

VRAM layout:

```
VRAM offset = y × 320 + x

0x0000  pixel (0, 0)
0x0140  pixel (0, 1)   [0x0140 = 320]
...
0xF9FF  pixel (319, 199)
0xFA00–0xFFFF  palette RAM (256 × 3 bytes = 768 bytes) [tentative]
```
