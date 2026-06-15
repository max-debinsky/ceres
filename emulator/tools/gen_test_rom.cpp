// gen_test_rom.cpp
// Generates a minimal test ROM that proves the CPU, memory, VRAM, and
// palette pipeline are all wired up correctly.
//
// What it does:
//   1. Sets palette[0] = black, palette[1] = white
//   2. Fills all 64,000 VRAM pixels with color 1 (white screen)
//   3. Calls STI then HALT
//
// Usage: gen_test_rom [output.rom]   (default: test.rom)
// Load:  ./ceres_emulator test.rom

#include <cstdint>
#include <cstdio>
#include <cstring>

using u8  = uint8_t;
using u16 = uint16_t;
using i8  = int8_t;
using i16 = int16_t;

static constexpr size_t ROM_BASE = 0x8000;
static constexpr size_t ROM_SIZE = 16384; // 16 KB

// ---------------------------------------------------------------------------
// Minimal ROM writer (all encodings from docs/isa/opcodes.md)
// ---------------------------------------------------------------------------

struct Rom {
    u8     buf[ROM_SIZE] = {};
    size_t cur = ROM_BASE;

    void emit(u16 val) {
        size_t off   = cur - ROM_BASE;
        buf[off]     = (u8)(val & 0xFF);
        buf[off + 1] = (u8)(val >> 8);
        cur += 2;
    }

    size_t here() const { return cur; }

    // 0x1  ADDI Rd, Ra, #imm6   (16-bit, imm6 signed −32..+31)
    void addi(u8 rd, u8 ra, i8 imm6) {
        emit((u16)(0x1000u | (rd << 9) | (ra << 6) | (imm6 & 0x3F)));
    }

    // 0x4  ALUL ADD Rd, Ra, #imm16   (32-bit)
    void add_imm16(u8 rd, u8 ra, u16 imm16) {
        emit((u16)(0x4000u | (rd << 9) | (ra << 6))); // op=ADD=0
        emit(imm16);
    }

    // 0x6  STORE [#addr16], Rs   (absolute, 32-bit: W=0, mode=0000, Ra=R0)
    void store_abs(u16 addr, u8 rs) {
        emit((u16)(0x6000u | (rs << 9)));
        emit(addr);
    }

    // 0xC  BCC: branch if condition holds, offset in words from next instruction
    //   cond: 0=BEQ 1=BNE 2=BLT 3=BGE 4=BLO 5=BHS 6=BMI 7=BPL
    void bcc(u8 cond, i16 offset_words) {
        emit((u16)(0xC000u | (cond << 9) | (offset_words & 0x1FF)));
    }
    void bne(i16 off) { bcc(1, off); }

    // 0xD  SYS
    void sti()  { emit(0xD001u); } // enable maskable interrupts (privileged)
    void halt() { emit(0xD000u); } // stop CPU (privileged)
};

// ---------------------------------------------------------------------------
// Boot sequence
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    const char* path = (argc > 1) ? argv[1] : "test.rom";

    Rom rom;

    // --- palette[0] = black ---
    rom.store_abs(0xFF03, 0);   // VID_PAL_IDX = 0   (R0 always = 0)
    rom.store_abs(0xFF04, 0);   // R = 0
    rom.store_abs(0xFF05, 0);   // G = 0
    rom.store_abs(0xFF06, 0);   // B = 0

    // --- palette[1] = white ---
    rom.addi(1, 0, 1);          // R1 = 1
    rom.store_abs(0xFF03, 1);   // VID_PAL_IDX = 1
    rom.add_imm16(3, 0, 255);   // R3 = 255
    rom.store_abs(0xFF04, 3);   // R = 255
    rom.store_abs(0xFF05, 3);   // G = 255
    rom.store_abs(0xFF06, 3);   // B = 255

    // --- fill VRAM with color 1 (white) ---
    rom.store_abs(0xFF01, 0);   // VID_ADDR = 0
    rom.add_imm16(2, 0, 64000); // R2 = 64000  (320 × 200 pixels)
    rom.addi(4, 0, 1);          // R4 = 1  (fill color)

    // Loop: write pixel, decrement counter, branch back while R2 != 0
    //
    //   fill_loop:                          ← address L
    //     STORE [0xFF02], R4   4 bytes      L+0
    //     ADDI  R2, R2, #-1   2 bytes      L+4
    //     BNE   fill_loop      2 bytes      L+6  → PC after fetch = L+8
    //
    // Offset = (L − (L+8)) / 2 = −4 words
    // store_abs=4B + addi=2B + bne=2B → PC after bne fetch = loop_start+8
    // offset = (loop_start − (loop_start+8)) / 2 = −4 words
    rom.store_abs(0xFF02, 4);   // vram[VID_ADDR++] = R4
    rom.addi(2, 2, -1);         // R2--  (sets Z when R2 reaches 0)
    rom.bne(-4);                // loop while Z=0

    // --- done ---
    rom.sti();
    rom.halt();

    // Write file
    FILE* f = fopen(path, "wb");
    if (!f) { perror("fopen"); return 1; }
    fwrite(rom.buf, 1, ROM_SIZE, f);
    fclose(f);

    printf("wrote %s  (%zu bytes, %zu used)\n",
           path, ROM_SIZE, rom.here() - ROM_BASE);
    return 0;
}
