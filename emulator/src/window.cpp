#include "window.hpp"

#include <SDL2/SDL.h>
#include <cstring>

#include <chrono>
#include <thread>

bool Window::init(int w, int h, int s, const char* title) {
    if (impl) return false;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return false;
    }

    impl = new Impl();

    width = w;
    height = h;
    scale = s;

    framebuffer.resize(static_cast<size_t>(width) * height);
    std::memset(framebuffer.data(), 0, framebuffer.size() * sizeof(uint32_t));

    impl->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width * scale,
        height * scale,
        SDL_WINDOW_SHOWN
    );

    if (!impl->window) {
        shutdown();
        return false;
    }

    impl->renderer = SDL_CreateRenderer(
        impl->window,
        -1,
        SDL_RENDERER_ACCELERATED
    );

    if (!impl->renderer) {
        shutdown();
        return false;
    }

    // crisp pixel scaling
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_RenderSetIntegerScale(impl->renderer, SDL_TRUE);
    SDL_RenderSetLogicalSize(impl->renderer, width, height);

    impl->texture = SDL_CreateTexture(
        impl->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );

    if (!impl->texture) {
        shutdown();
        return false;
    }

    return true;
}

uint32_t* Window::pixels() {
    return framebuffer.data();
}

bool Window::pollEvents() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
    }

    return true;
}

void Window::present() {
    if (!impl) return;

    SDL_UpdateTexture(
        impl->texture,
        nullptr,
        framebuffer.data(),
        width * sizeof(uint32_t)
    );

    SDL_RenderClear(impl->renderer);

    SDL_RenderCopy(
        impl->renderer,
        impl->texture,
        nullptr,
        nullptr
    );

    SDL_RenderPresent(impl->renderer);
}

void Window::frameLimit(int fps) {
    static auto last = std::chrono::high_resolution_clock::now();

    auto frameTime = std::chrono::milliseconds(1000 / fps);
    auto now = std::chrono::high_resolution_clock::now();

    auto elapsed = now - last;

    if (elapsed < frameTime) {
        std::this_thread::sleep_for(frameTime - elapsed);
    }

    last = std::chrono::high_resolution_clock::now();
}

void Window::shutdown() {
    if (!impl) return;

    if (impl->texture) SDL_DestroyTexture(impl->texture);
    if (impl->renderer) SDL_DestroyRenderer(impl->renderer);
    if (impl->window) SDL_DestroyWindow(impl->window);

    delete impl;
    impl = nullptr;

    framebuffer.clear();

    SDL_Quit();
}

Window::~Window() {
    shutdown();
}