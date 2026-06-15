#include "display.h"

void render_frame(const Memory& mem, uint32_t* out_pixels) {
    for (int i = 0; i < Memory::SCREEN_W * Memory::SCREEN_H; i++) {
        uint8_t idx      = mem.vram[i];
        uint8_t r        = mem.palette[idx][0];
        uint8_t g        = mem.palette[idx][1];
        uint8_t b        = mem.palette[idx][2];
        out_pixels[i]    = 0xFF000000u
                         | ((uint32_t)r << 16)
                         | ((uint32_t)g <<  8)
                         |  (uint32_t)b;
    }
}
