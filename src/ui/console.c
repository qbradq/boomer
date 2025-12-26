#include "console.h"
#include "../core/config.h"
#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define MAX_LOG_LINES 128
#define MAX_LINE_LEN 256

#define CONSOLE_HEIGHT 720
#define CONSOLE_WIDTH 1280

typedef enum {
    CONSOLE_HIDDEN,
    CONSOLE_HALF,
    CONSOLE_FULL
} ConsoleState;

static struct {
    RenderTexture2D target;
    Font font;
    
    ConsoleState state;
    float anim_t; // 0.0 to 1.0 (Openness)
    float target_t;
    
    char log[MAX_LOG_LINES][MAX_LINE_LEN];
    int log_head; // Index of next write
    int log_scroll; // Scroll offset from bottom
    
    bool map_loaded;
} console = {0};

static bool g_map_loaded = false;

void Console_SetMapLoaded(bool loaded) {
    g_map_loaded = loaded;
}

// Helper to get color from config u32 (0xAABBGGRR or similar)
static Color Console_GetColor(u32 hex) {
    // Assuming config is 0xRRGGBBAA or similar.
    // Let's assume standard u32 hex: 0xRRGGBBAA
    unsigned char r = (hex >> 24) & 0xFF;
    unsigned char g = (hex >> 16) & 0xFF;
    unsigned char b = (hex >> 8) & 0xFF;
    unsigned char a = hex & 0xFF;
    return (Color){r, g, b, a};
}

bool Console_Init(void) {
    console.target = LoadRenderTexture(CONSOLE_WIDTH, CONSOLE_HEIGHT);
    
    const GameConfig* cfg = Config_Get();
    // Try to load configured font
    if (FileExists(cfg->console_font_path)) {
        console.font = LoadFontEx(cfg->console_font_path, cfg->console_font_size, NULL, 0);
    } else {
        printf("Console: Font '%s' not found, using default.\n", cfg->console_font_path);
        console.font = GetFontDefault();
    }
    
    console.state = CONSOLE_FULL; // Default start full
    console.target_t = 1.0f;
    console.anim_t = 1.0f;
    
    return true;
}

void Console_Shutdown(void) {
    if (console.target.id != 0) UnloadRenderTexture(console.target);
    if (console.font.texture.id != 0) UnloadFont(console.font);
}

void Console_Log(const char* fmt, ...) {
    char buf[MAX_LINE_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    strncpy(console.log[console.log_head], buf, MAX_LINE_LEN);
    console.log_head = (console.log_head + 1) % MAX_LOG_LINES;
    
    printf("[CONSOLE] %s\n", buf); // Also stdout
}

bool Console_HandleEvent(void) {
    if (IsKeyPressed(KEY_GRAVE)) { // Tilde / Backquote
        // Toggle Logic
        if (!g_map_loaded) {
            // Cannot dismiss if no map
            return true; 
        }
        
        if (console.state == CONSOLE_FULL) {
            console.state = CONSOLE_HIDDEN;
            console.target_t = 0.0f;
        } else if (console.state == CONSOLE_HIDDEN) {
            console.state = CONSOLE_HALF;
            console.target_t = 0.5f;
        } else if (console.state == CONSOLE_HALF) {
            console.state = CONSOLE_FULL;
            console.target_t = 1.0f;
        }
        return true;
    }
    
    if (console.state != CONSOLE_HIDDEN) {
        return true; 
    }
    
    return false;
}

void Console_Update(float dt) {
    // Animate
    float speed = 5.0f * dt;
    if (console.anim_t < console.target_t) {
        console.anim_t += speed;
        if (console.anim_t > console.target_t) console.anim_t = console.target_t;
    } else if (console.anim_t > console.target_t) {
        console.anim_t -= speed;
        if (console.anim_t < console.target_t) console.anim_t = console.target_t;
    }
}

void Console_Draw(void) {
    if (console.anim_t <= 0.01f) return;
    
    const GameConfig* cfg = Config_Get();
    Color bg = Console_GetColor(cfg->console_bg_color);
    Color txt_col = Console_GetColor(cfg->console_text_color);
    
    int visible_h = (int)(CONSOLE_HEIGHT * console.anim_t);
    
    BeginTextureMode(console.target);
        ClearBackground(BLANK); // Clear to transparent
        
        // Draw background for visible area
        // We draw the full console height logic, but we might only want to show part.
        // Actually, if we draw to texture, we can just draw normally and render the texture cropped or scaled.
        // But the animation describes "opening".
        // Let's draw the full console background and text to the texture, 
        // then draw the texture with a source/dest rect in end.
        
        // Fill background
        DrawRectangle(0, 0, CONSOLE_WIDTH, visible_h, bg);
    
        // Draw Text
        // Draw from log_head backwards, starting from bottom of visible area
        int y = visible_h - 10;
        int line_h = console.font.baseSize; // Approximation
        
        int idx = (console.log_head - 1 - console.log_scroll + MAX_LOG_LINES) % MAX_LOG_LINES;
        int count = 0;
        
        while (y > 0 && count < MAX_LOG_LINES) {
            if (console.log[idx][0] != 0) {
                DrawTextEx(console.font, console.log[idx], (Vector2){10, (float)(y - line_h)}, (float)console.font.baseSize, 1.0f, txt_col);
            }
            y -= line_h;
            idx = (idx - 1 + MAX_LOG_LINES) % MAX_LOG_LINES;
            count++;
        }
        
    EndTextureMode();
    
    // Draw the render texture to the screen
    // We can draw it effectively as an overlay.
    // Flip Y because of OpenGL coordinates in RenderTexture
    DrawTextureRec(console.target.texture, (Rectangle){0, 0, (float)CONSOLE_WIDTH, (float)-CONSOLE_HEIGHT}, (Vector2){0, 0}, WHITE);
}

bool Console_IsActive(void) {
    return console.state != CONSOLE_HIDDEN;
}
