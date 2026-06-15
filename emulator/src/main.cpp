#include "cpu.h"
#include "memory.h"
#include "display.h"
#include "../include/window.hpp"
#include <cstdio>
#include <cstdlib>

static constexpr int CYCLES_PER_FRAME = 16667; // ~1 MHz @ 60 fps
static constexpr int SCALE            = 3;

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

    Window window;
    if (!window.init(Memory::SCREEN_W, Memory::SCREEN_H, SCALE, "Ceres")) {
        fprintf(stderr, "Window init failed\n");
        return 1;
    }

    bool running = true;
    while (running) {
        for (int i = 0; i < CYCLES_PER_FRAME; i++)
            cpu.step(mem);

        if (mem.reset_requested) {
            mem.reset_requested = false;
            cpu.reset();
        }

        render_frame(mem, window.pixels());
        window.present();
        window.frameLimit(60);

        // TODO: expose key events from Window::pollEvents and call
        //   mem.push_key(scancode, is_down) here
        running = window.pollEvents();
    }

    return 0;
}
