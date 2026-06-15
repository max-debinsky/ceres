# Graphics system

CPU-16 uses a pure framebuffer display: 320×200 pixels, 256 indexed colors, one byte per pixel. There are no hardware sprites, tiles, or blitter.

---

## Display parameters

| Property | Value |
|----------|-------|
| Resolution | 320 × 200 |
| Color depth | 8 bits per pixel (indexed) |
| Palette | 256 entries × 24-bit RGB |
| Framebuffer size | 320 × 200 = 64,000 bytes |
| VRAM chip size | 65,536 bytes (64 KB) |

---

## VRAM layout

VRAM is a dedicated 64 KB chip connected to the video controller. It is **outside the CPU's 64 KB address space** — ordinary LOAD and STORE cannot reach it. The CPU communicates with VRAM only through the video controller registers at 0xFF00–0xFF07.

```
VRAM 0x0000 → pixel (0, 0)         top-left
VRAM 0x013F → pixel (319, 0)       top-right  [0x013F = 319]
VRAM 0x0140 → pixel (0, 1)         start of row 1  [0x0140 = 320]
...
VRAM 0xF9FF → pixel (319, 199)     bottom-right

VRAM 0xFA00–0xFCFF → palette RAM   (256 entries × 3 bytes = 768 bytes)
VRAM 0xFD00–0xFFFF → reserved
```

Pixel address formula:

```
vram_offset = y × 320 + x
```

---

## Palette

The palette maps each 8-bit pixel value (0–255) to a 24-bit output color (8 bits R, 8 bits G, 8 bits B). The default palette layout will be defined when the BASIC ROM is designed.

Palette RAM lives in VRAM at 0xFA00–0xFCFF. Each entry occupies 3 consecutive bytes: R, G, B. The CPU updates palette entries through the VID_PAL_IDX / VID_PAL_R / VID_PAL_G / VID_PAL_B registers (see [`io.md`](../isa/io.md)).

---

## Pixel operations

### Writing a single pixel

```asm
; Write color index 7 to pixel (x=50, y=30)
; offset = 30 * 320 + 50 = 9650 = 0x25B2
MOV   R1, #0x25B2
STORE [0xFF01], R1     ; VID_ADDR = 0x25B2
MOV   R1, #7
STORE [0xFF02], R1     ; write color byte; VID_ADDR advances to 0x25B3
```

### Clearing the screen

```asm
; fill framebuffer with color 0 (typically black)
MOV   R1, #0
STORE [0xFF01], R1     ; VID_ADDR = 0
MOV   R2, #64000       ; 320 × 200 pixels
MOV   R3, #0
clear_loop:
  STORE [0xFF02], R3   ; write pixel, VID_ADDR auto-increments
  SUB   R2, R2, #1
  BNE   clear_loop
```

64,000 loop iterations is the baseline cost of a full-screen clear. ROM will provide an optimized block-fill routine (TBD).

### Drawing a horizontal line

```asm
; horizontal line at y=10, x=0..319, color 15
MOV   R1, #3200        ; offset = 10 * 320
STORE [0xFF01], R1
MOV   R2, #320
MOV   R3, #15
hline:
  STORE [0xFF02], R3
  SUB   R2, R2, #1
  BNE   hline
```

Horizontal lines are sequential in VRAM and benefit directly from VID_ADDR auto-increment. Vertical lines require stepping by 320 per pixel and must set VID_ADDR individually for each write.

---

## V-blank

The video controller generates a V-blank period between frames — the interval when the display is not actively rendering pixels. During V-blank, VRAM is not being read by the video controller, making it the ideal time to update the framebuffer without visible tearing.

V-blank can be detected two ways:

**Polling:**
```asm
wait_vblank:
  LOAD  R1, [0xFF07]        ; VID_STATUS
  AND   R1, R1, #0x0001     ; bit 0 = V-blank active
  BEQ   wait_vblank
```

**Interrupt:** Set bit 1 of VID_CTRL to generate IRQ 6 at the start of each V-blank. The interrupt handler then performs framebuffer updates.

---

## Double buffering

The framebuffer is 64,000 bytes. VRAM is 65,536 bytes. There is not enough room in VRAM for two complete framebuffers (64,000 × 2 = 128,000 bytes).

Options for tear-free animation:

1. **Update during V-blank only.** Draw changes directly into VRAM, but only during the V-blank window. Simple and works for slow updates.
2. **Partial buffering.** Buffer a horizontal band in system RAM and blit it to VRAM during V-blank.
3. **Accept tearing.** For full-speed full-screen updates where tearing is acceptable (e.g., fast scrolling games).

Full double buffering would require a 128 KB VRAM chip — a possible hardware revision if needed.

---

## Software sprites

There are no hardware sprites. The typical software sprite approach:

1. Compute the sprite's VRAM position.
2. Read and save the background pixels the sprite will cover (into a RAM buffer).
3. Write sprite pixels into VRAM.
4. On the next frame, restore the saved background before drawing the sprite at its new position.

ROM will provide sprite draw/erase primitives (TBD).

---

## Design rationale

| Choice | Reason |
|--------|--------|
| Pure framebuffer | Maximum programmer control; sprites are a software convention, not a hardware constraint |
| 8bpp indexed | One byte per pixel — trivial VRAM address arithmetic; palette enables 16M possible colors |
| VRAM outside CPU space | Full 64 KB of RAM available to programs; no tradeoff between code/data and framebuffer |
| 320×200 | 64,000 bytes fits in 64 KB VRAM with room for the palette; classic home computer resolution |
| No DMA | Adds significant complexity; excluded from v1. ROM routines cover the common fill and blit cases |
