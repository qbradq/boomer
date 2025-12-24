#include "video.h"
#include "texture.h"
#include <SDL.h>
#include <stdio.h>

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;

// Exposed buffer
static u32 frame_buffer[VIDEO_WIDTH * VIDEO_HEIGHT];
u32* video_pixels = frame_buffer;

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
    u32 c = (color.a << 24) | (color.b << 16) | (color.g << 8) | color.r;
    for (int i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; ++i) {
        video_pixels[i] = c;
    }
}

void Video_PutPixel(int x, int y, Color color) {
    if (x < 0 || x >= VIDEO_WIDTH || y < 0 || y >= VIDEO_HEIGHT) return;
    video_pixels[y * VIDEO_WIDTH + x] = (color.a << 24) | (color.b << 16) | (color.g << 8) | color.r;
}

void Video_Present(void) {
    SDL_UpdateTexture(texture, NULL, video_pixels, VIDEO_WIDTH * sizeof(u32));
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

void Video_DrawVertLine(int x, int y1, int y2, Color color) {
    if (x < 0 || x >= VIDEO_WIDTH) return;
    
    if (y1 > y2) {
        int temp = y1;
        y1 = y2;
        y2 = temp;
    }
    
    if (y2 < 0 || y1 >= VIDEO_HEIGHT) return;
    
    if (y1 < 0) y1 = 0;
    if (y2 >= VIDEO_HEIGHT) y2 = VIDEO_HEIGHT - 1;
    
    u32 c = (color.a << 24) | (color.b << 16) | (color.g << 8) | color.r;
    
    for (int y = y1; y <= y2; ++y) {
        video_pixels[y * VIDEO_WIDTH + x] = c;
    }
}

void Video_DrawTexturedColumn(int x, int y_start, int y_end, Texture* tex, int tex_x, float v_start, float v_step) {
    if (x < 0 || x >= VIDEO_WIDTH) return;
    
    int y1 = y_start;
    int y2 = y_end;
    
    // Don't swap here, assume y_start is "top" and y_end is "bottom" in standard coords...
    // But rendering goes from top to bottom on screen.
    // If y1 > y2, it's inverted, but let's assume valid range.
    if (y1 > y2) return; 

    // Clip Top
    if (y1 < 0) {
        v_start += (-y1) * v_step; // Advance V
        y1 = 0;
    }
    if (y1 >= VIDEO_HEIGHT) return;
    
    // Clip Bottom
    if (y2 >= VIDEO_HEIGHT) y2 = VIDEO_HEIGHT - 1;
    if (y2 < 0) return;
    
    float v = v_start;
    u32 th = tex->height;
    u32 tw = tex->width;
    u32* tex_pixels = tex->pixels; // Renamed to avoid conflict with global 'pixels'
    
    // Wrap tex_x just in case
    tex_x = tex_x % tw;
    
    for (int y = y1; y <= y2; ++y) {
        // Sample
        u32 tex_y = (u32)v % th;
        u32 color = tex_pixels[tex_y * tw + tex_x];
        
        // Plot (Copy u32 directly)
        video_pixels[y * VIDEO_WIDTH + x] = color; // Changed to global 'pixels'
        
        v += v_step;
    }
}
