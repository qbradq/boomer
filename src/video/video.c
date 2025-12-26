#include "video.h"
#include "texture.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h> // abs

// Exposed buffer
static u32 frame_buffer[VIDEO_WIDTH * VIDEO_HEIGHT];
u32* video_pixels = frame_buffer;

static int current_scale = 4;
static bool is_fullscreen = false;

// Raylib specific
static Texture2D screen_texture;
static bool texture_ready = false;

bool Video_Init(const char* title) {
    // Raylib Init
    SetTraceLogLevel(LOG_WARNING); // Reduce noise
    
    // We want the window to be scalable
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    
    int width = VIDEO_WIDTH * current_scale;
    int height = VIDEO_HEIGHT * current_scale;
    
    InitWindow(width, height, title);
    
    if (!IsWindowReady()) {
        printf("Video: Raylib InitWindow failed.\n");
        return false;
    }
    
    // Create a texture to dump our framebuffer into
    // Raylib LoadTextureFromImage requires an Image.
    // We'll create a blank image first or just create texture entirely?
    // GenTexture is not exposed directly for empty.
    // We use LoadTextureFromImage with a blank image.
    Image img = GenImageColor(VIDEO_WIDTH, VIDEO_HEIGHT, BLACK);
    screen_texture = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(screen_texture, TEXTURE_FILTER_POINT); // Pixel art style
    
    texture_ready = true;

    return true;
}

void Video_Shutdown(void) {
    if (texture_ready) UnloadTexture(screen_texture);
    CloseWindow();
}

void Video_ChangeScale(int delta) {
    if (IsWindowFullscreen()) return;

    current_scale += delta;
    if (current_scale < 1) current_scale = 1;
    if (current_scale > 12) current_scale = 12;

    int w = VIDEO_WIDTH * current_scale;
    int h = VIDEO_HEIGHT * current_scale;
    
    SetWindowSize(w, h);
    // Center window logic is a bit manual in Raylib or we assume OS handles it.
    // Raylib doesn't have explicit "Center Window" command easily exposed without getting monitor info.
    int monitor = GetCurrentMonitor();
    int mx = GetMonitorWidth(monitor);
    int my = GetMonitorHeight(monitor);
    SetWindowPosition((mx - w)/2, (my - h)/2);
    
    printf("Window Scale: %dx (%dx%d)\n", current_scale, w, h);
}

void Video_ToggleFullscreen(void) {
    ToggleFullscreen(); // Raylib handles this
    is_fullscreen = IsWindowFullscreen();
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

void Video_BeginFrame(void) {
    // Prepare for drawing
    // In our case, we might update texture here?
    // Raylib's BeginDrawing() clears screen usually.
    BeginDrawing();
    ClearBackground(BLACK); // Clear "back" of window
}

void Video_DrawGame(void* dst_rect) {
    (void)dst_rect; // Ignored for now
    
    // Update GPU texture from CPU buffer
    UpdateTexture(screen_texture, video_pixels);
    
    // Draw texture scaled to window
    // Calculate integer scaling
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    
    float scale = (screenW / VIDEO_WIDTH) < (screenH / VIDEO_HEIGHT) ? (screenW / VIDEO_WIDTH) : (screenH / VIDEO_HEIGHT);
    if (scale < 1.0f) scale = 1.0f;
    // Floor scale for integer scaling if desired, or keep specific aspect ratio
    scale = (float)(int)scale; // Enforce integer scaling
    
    float viewW = VIDEO_WIDTH * scale;
    float viewH = VIDEO_HEIGHT * scale;
    
    float x = (screenW - viewW) * 0.5f;
    float y = (screenH - viewH) * 0.5f;
    
    Rectangle src = { 0, 0, (float)VIDEO_WIDTH, (float)VIDEO_HEIGHT };
    Rectangle dst = { x, y, viewW, viewH };
    
    DrawTexturePro(screen_texture, src, dst, (Vector2){0,0}, 0.0f, WHITE);
}

void Video_EndFrame(void) {
    EndDrawing();
}

void Video_Present(void) {
    Video_BeginFrame();
    Video_DrawGame(NULL);
    Video_EndFrame();
}

// Primitives directly into pixel buffer (Software Rendering)
void Video_DrawLine(int x0, int y0, int x1, int y1, Color color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
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

void Video_DrawTexturedColumn(int x, int y_start, int y_end, struct GameTexture* tex, int tex_x, float v_start, float v_step) {
    if (x < 0 || x >= VIDEO_WIDTH) return;
    
    int y1 = y_start;
    int y2 = y_end;
    
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
    u32* tex_pixels = tex->pixels; 
    
    tex_x = tex_x % tw; // Wrap width
    
    for (int y = y1; y <= y2; ++y) {
        int tex_y = (int)v % th;
        u32 color = tex_pixels[tex_y * tw + tex_x];
        
        video_pixels[y * VIDEO_WIDTH + x] = color;
        
        v += v_step;
    }
}
