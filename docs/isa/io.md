# I/O

CPU-16 has no special I/O instructions. All peripheral access uses ordinary LOAD and STORE to the **memory-mapped I/O region at 0xFF00–0xFFFF**.

Because I/O registers live in the normal address space, any addressing mode works. Register reads may have side effects (e.g., reading the keyboard data register clears the "key available" flag).

---

## Video controller — 0xFF00–0xFF07

The video controller bridges the CPU and the VRAM chip. VRAM is outside the CPU's 64 KB address space; all framebuffer access goes through these registers. See [`../architecture/graphics.md`](../architecture/graphics.md) for the full display model.

| Address | Name | Access | Description |
|---------|------|--------|-------------|
| 0xFF00 | VID_CTRL | R/W | Control register |
| 0xFF01 | VID_ADDR | R/W | VRAM address (16-bit) |
| 0xFF02 | VID_DATA | R/W | Read/write one byte at VRAM[VID_ADDR]; auto-increments VID_ADDR |
| 0xFF03 | VID_PAL_IDX | W | Palette entry to write (0–255) |
| 0xFF04 | VID_PAL_R | W | Palette red component (0–255) |
| 0xFF05 | VID_PAL_G | W | Palette green component (0–255) |
| 0xFF06 | VID_PAL_B | W | Palette blue component (0–255) |
| 0xFF07 | VID_STATUS | R | Status: bit 0 = V-blank active |

### VID_CTRL bits

| Bit | Name | Description |
|-----|------|-------------|
| 0 | DISPLAY_EN | 1 = display active; 0 = screen blanked |
| 1 | VSYNC_IRQ | 1 = generate interrupt at the start of each V-blank |
| 2–15 | — | Reserved (write 0) |

### Writing a pixel

```asm
; Write color index 0x2A to pixel (x=10, y=5)
; VRAM offset = 5 * 320 + 10 = 1610 = 0x064A
MOV   R1, #0x064A
STORE [0xFF01], R1     ; set VRAM address
MOV   R1, #0x2A
STORE [0xFF02], R1     ; write color byte (VID_ADDR advances to 0x064B)
```

Only the low byte of a STORE to VID_DATA is used. The high byte is ignored.

### Sequential pixel writes

VID_ADDR auto-increments by 1 after each access to VID_DATA. A run of pixels can be written with repeated stores without resetting the address each time:

```asm
; fill 320 pixels (one horizontal line) at y=10 with color 0x0F
MOV   R1, #3200        ; offset = 10 * 320
STORE [0xFF01], R1
MOV   R2, #320
MOV   R3, #0x0F
line_loop:
  STORE [0xFF02], R3
  SUB   R2, R2, #1
  BNE   line_loop
```

VID_ADDR wraps from 0xFFFF to 0x0000.

### Setting a palette entry

```asm
; palette[5] = (255, 0, 0)   red
MOV   R1, #5
STORE [0xFF03], R1     ; select palette index 5
MOV   R1, #255
STORE [0xFF04], R1     ; R = 255
MOV   R1, #0
STORE [0xFF05], R1     ; G = 0
STORE [0xFF06], R1     ; B = 0
```

---

## Keyboard — 0xFF10–0xFF12

| Address | Name | Access | Description |
|---------|------|--------|-------------|
| 0xFF10 | KBD_DATA | R | Scancode of the most recent key event |
| 0xFF11 | KBD_STATUS | R | Bit 0 = key event available; bit 1 = key down (1) / up (0) |
| 0xFF12 | KBD_CTRL | W | Bit 0 = enable keyboard interrupt (IRQ 6) |

Reading KBD_DATA clears the "key available" bit in KBD_STATUS.

### Polling for a keypress

```asm
wait_key:
  LOAD  R1, [0xFF11]
  AND   R1, R1, #0x0001     ; bit 0 = key available
  BEQ   wait_key
  LOAD  R1, [0xFF10]        ; read scancode; clears the flag
  ; check bit 1 of KBD_STATUS for key-down vs key-up if needed
```

### Interrupt-driven input

```asm
; enable keyboard interrupt
MOV   R1, #0x0001
STORE [0xFF12], R1
STI                          ; ensure interrupts are enabled
```

The keyboard IRQ handler reads KBD_DATA to get the scancode and KBD_STATUS bit 1 to determine key-down vs key-up.

---

## Timer — 0xFF20–0xFF22

One 16-bit countdown timer. A second timer may be added in a future revision.

| Address | Name | Access | Description |
|---------|------|--------|-------------|
| 0xFF20 | TMR_COUNT | R | Current counter value (counts down) |
| 0xFF21 | TMR_RELOAD | R/W | Reload value (loaded into TMR_COUNT on expiry in periodic mode) |
| 0xFF22 | TMR_CTRL | R/W | Control register |

### TMR_CTRL bits

| Bit | Name | Description |
|-----|------|-------------|
| 0 | ENABLE | 1 = timer running; 0 = stopped |
| 1 | MODE | 0 = one-shot (stops at 0); 1 = periodic (reloads and continues) |
| 2 | IRQ_EN | 1 = trigger IRQ 4 when counter reaches 0 |
| 3 | EXPIRED | Read-only; 1 = timer has reached 0 since last read; clears on read |

### Setting up a periodic interrupt

```asm
; fire IRQ 4 every 10000 timer ticks
MOV   R1, #10000
STORE [0xFF21], R1        ; set reload value
MOV   R1, #10000
STORE [0xFF22], R1        ; prime the counter (write to CTRL also loads count? TBD)
MOV   R1, #0b00000111     ; ENABLE | MODE=periodic | IRQ_EN
STORE [0xFF22], R1
```

The timer tick rate (ticks per second) depends on the system clock frequency and will be specified when the emulator is designed.

---

## Audio — 0xFF30–0xFF3F

Reserved. The audio system is not yet designed.

---

## System — 0xFFF0–0xFFFF

| Address | Name | Access | Description |
|---------|------|--------|-------------|
| 0xFFFF | SYS_RESET | W | Write any value to trigger a software reset |

A software reset has the same effect as the hardware reset line: the CPU jumps to vector 0, registers are reset to their initial state, and the boot sequence runs again. RAM is not cleared.

---

## I/O access rules

- All I/O register addresses are word-aligned. Unaligned access is undefined.
- Reading a write-only register returns an unspecified value.
- Writing a read-only register has no effect.
- Reads from some registers have side effects (KBD_DATA, TMR_CTRL bit 3). Document these in handlers.
