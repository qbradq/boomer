#include "video.h"
#include <SDL.h>
#include <stdio.h>

static SDL_Window*   window   = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture*  texture  = NULL;
static u32           pixels[VIDEO_WIDTH * VIDEO_HEIGHT];

bool Video_Init(const char* title) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    int window_width  = VIDEO_WIDTH * VIDEO_SCALE;
    int window_height = VIDEO_HEIGHT * VIDEO_SCALE;

    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              window_width, window_height,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_STREAMING,
                                VIDEO_WIDTH, VIDEO_HEIGHT);
    if (!texture) {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void Video_Shutdown(void) {
    if (texture)  SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    SDL_Quit();
}

void Video_Clear(Color color) {
    u32 pixel_val = (color.a << 24) | (color.b << 16) | (color.g << 8) | color.r;
    for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; ++i) {
        pixels[i] = pixel_val;
    }
}

void Video_PutPixel(int x, int y, Color color) {
    if (x < 0 || x >= VIDEO_WIDTH || y < 0 || y >= VIDEO_HEIGHT) return;
    pixels[y * VIDEO_WIDTH + x] = (color.a << 24) | (color.b << 16) | (color.g << 8) | color.r;
}

void Video_Present(void) {
    SDL_UpdateTexture(texture, NULL, pixels, VIDEO_WIDTH * sizeof(u32));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void Video_DrawLine(int x0, int y0, int x1, int y1, Color color) {
    int dx =abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        Video_PutPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Video_DrawVertLine(int x, int y0, int y1, Color color) {
    if (x < 0 || x >= VIDEO_WIDTH) return;
    if (y0 > y1) { int temp = y0; y0 = y1; y1 = temp; }
    if (y0 < 0) y0 = 0;
    if (y1 >= VIDEO_HEIGHT) y1 = VIDEO_HEIGHT - 1;
    if (y0 > y1) return;

    u32 pixel = (color.a << 24) | (color.b << 16) | (color.g << 8) | color.r;
    u32* dest = &pixels[y0 * VIDEO_WIDTH + x];
    // Simple loop is fast enough for software 320x200
    for (int y = y0; y <= y1; ++y) {
        *dest = pixel;
        dest += VIDEO_WIDTH;
    }
}

