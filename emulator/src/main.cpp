#include "cpu.h"
#include "memory.h"
#include "display.h"
#include <cstdio>
#include <cstdlib>

// TODO: include your display library header
// #include "your_display.h"

// Approximate cycles to run per frame. At 1 MHz and 60 fps = ~16667 cycles.
static constexpr int CYCLES_PER_FRAME = 16667;

static void load_rom(Memory& mem, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open ROM: %s\n", path); return; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (buf) {
        fread(buf, 1, (size_t)sz, f);
        mem.load_rom(buf, (size_t)sz);
        free(buf);
    }
    fclose(f);
}

int main(int argc, char* argv[]) {
    Memory mem;
    CPU    cpu;

    if (argc > 1) load_rom(mem, argv[1]);

    cpu.reset();

    // TODO: initialize your display library, e.g.:
    // YourDisplay display(Memory::SCREEN_W, Memory::SCREEN_H, "Ceres");

    static uint32_t pixels[Memory::SCREEN_W * Memory::SCREEN_H];

    bool running = true;
    while (running) {
        // --- Run one frame of CPU cycles ---
        for (int i = 0; i < CYCLES_PER_FRAME; i++) {
            cpu.step(mem);
        }

        // --- Handle software reset request ---
        if (mem.reset_requested) {
            mem.reset_requested = false;
            cpu.reset();
        }

        // --- Convert VRAM to ARGB and present ---
        render_frame(mem, pixels);
        // TODO: pass to your display library, e.g.:
        // display.present(pixels);

        // --- Feed input events to the emulator ---
        // TODO: poll events from your display library, then call:
        //   mem.push_key(scancode, is_down);
        //   if (quit_event) running = false;
    }

    return 0;
}
