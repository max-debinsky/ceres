#pragma once

#include <cstdint>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class Window {
public:
    bool init(int logicalW, int logicalH, int scale, const char* title);
    bool pollEvents();
    void present();
    void frameLimit(int fps);
    void shutdown();

    uint32_t* pixels();

    ~Window();

private:
    struct Impl {
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        SDL_Texture* texture = nullptr;
    };

    Impl* impl = nullptr;

    int width = 0;
    int height = 0;
    int scale = 1;

    std::vector<uint32_t> framebuffer;
};