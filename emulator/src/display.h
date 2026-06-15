#pragma once
#include <cstdint>
#include "memory.h"

// Converts the VRAM framebuffer + palette into an array of 0xAARRGGBB pixels.
// out_pixels must be at least SCREEN_W * SCREEN_H (320*200 = 64000) elements.
void render_frame(const Memory& mem, uint32_t* out_pixels);
