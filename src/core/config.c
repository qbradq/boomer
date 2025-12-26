#include "config.h"
#include "fs.h"
#include "../core/script_sys.h" // Access to QuickJS headers
#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GameConfig g_config = {
    .logical_width = 320,
    .logical_height = 180,
    .window_scale = 3,
    .fullscreen = false,
    .console_bg_color = 0x000000AA,
    .console_text_color = 0xFFFFFFFF,
    .console_font_path = "fonts/unscii-8-thin.ttf",
    .console_font_size = 8
};

static u32 ParseColor(const char* hex_str) {
    if (!hex_str || hex_str[0] != '#') return 0; // Return 0 (Transparent) if not a color code
    
    // #RRGGBB or #RRGGBBAA
    unsigned int r, g, b, a = 255;
    int len = strlen(hex_str);
    if (len == 7) {
        sscanf(hex_str + 1, "%02x%02x%02x", &r, &g, &b);
    } else if (len == 9) {
        sscanf(hex_str + 1, "%02x%02x%02x%02x", &r, &g, &b, &a);
    } else {
        return 0xFFFFFFFF;
    }
    
    // Return ABGR for SDL usually? Or specific packed format?
    // Let's assume 0xRRGGBBAA and we'll convert when drawing if needed
    // or use SDL_MapRGBA.
    // For now: 0xRRGGBBAA
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static void LoadConfigObj(JSContext* ctx, JSValue obj) {
    JSValue res = JS_GetPropertyStr(ctx, obj, "logical_resolution");
    // The original line below had a syntax error and was redundant with the next 'if'.
    // The comment indicated JS_IsArray should only take one argument.
    // if (JS_IsArray(ctx, res) == 1 || JS_IsArray(ctx, res) == true) { // Wait, JS_IsArray returns int (bool)
    //    // Error said: too many arguments. JS_IsArray(val).
    //    // Also it returns int (0/1).
    //    // So:
    if (JS_IsArray(res)) {
        JSValue w = JS_GetPropertyUint32(ctx, res, 0);
        JSValue h = JS_GetPropertyUint32(ctx, res, 1);
        int iw, ih;
        if (JS_ToInt32(ctx, &iw, w) == 0) g_config.logical_width = iw;
        if (JS_ToInt32(ctx, &ih, h) == 0) g_config.logical_height = ih;
        JS_FreeValue(ctx, w);
        JS_FreeValue(ctx, h);
    }
    JS_FreeValue(ctx, res);

    JSValue scale = JS_GetPropertyStr(ctx, obj, "window_size");
    if (!JS_IsUndefined(scale)) {
        int s;
        if (JS_ToInt32(ctx, &s, scale) == 0) g_config.window_scale = s;
    }
    JS_FreeValue(ctx, scale);

    JSValue full = JS_GetPropertyStr(ctx, obj, "fullscreen");
    if (JS_IsBool(full)) {
        g_config.fullscreen = JS_ToBool(ctx, full);
    }
    JS_FreeValue(ctx, full);
    
    // Console
    JSValue bg = JS_GetPropertyStr(ctx, obj, "console_background");
    if (JS_IsString(bg)) {
        const char* s = JS_ToCString(ctx, bg);
        u32 col = ParseColor(s);
        if (col != 0) g_config.console_bg_color = col;
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, bg);
    
    JSValue txt = JS_GetPropertyStr(ctx, obj, "console_text");
    if (JS_IsString(txt)) {
        const char* s = JS_ToCString(ctx, txt);
        g_config.console_text_color = ParseColor(s);
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, txt);
    
    JSValue font = JS_GetPropertyStr(ctx, obj, "console_font");
    if (JS_IsString(font)) {
        const char* s = JS_ToCString(ctx, font);
        strncpy(g_config.console_font_path, s, sizeof(g_config.console_font_path) - 1);
        JS_FreeCString(ctx, s);
    }
    JS_FreeValue(ctx, font);
    
    JSValue fsize = JS_GetPropertyStr(ctx, obj, "console_font_size");
    if (JS_IsNumber(fsize)) {
        int sz;
        if (JS_ToInt32(ctx, &sz, fsize) == 0) g_config.console_font_size = sz;
    }
    JS_FreeValue(ctx, fsize);
}

static bool LoadJSONFile(JSContext* ctx, const char* path) {
    size_t size;
    char* data = FS_ReadFile(path, &size);
    if (!data) return false;
    
    JSValue val = JS_ParseJSON(ctx, data, size, path);
    FS_FreeFile(data);
    
    if (JS_IsException(val)) {
        printf("Config: Invalid JSON in '%s'\n", path);
        JS_FreeValue(ctx, val);
        return false;
    }
    
    if (JS_IsObject(val)) {
        LoadConfigObj(ctx, val);
    }
    
    JS_FreeValue(ctx, val);
    return true;
}

bool Config_Load() {
    // We use a temporary QJS runtime for config loading
    // to keep it decoupled from the game script system.
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);
    
    if (!ctx) return false;
    
    char path[256];
    
    // 1. Base Config
    if (LoadJSONFile(ctx, "config.json")) {
        printf("Config: Loaded base config.json\n");
    } else {
        printf("Config: Failed to load base config.json\n");
    }
    
    // 2. User Config (if user data is mounted)
    // We assume FS_ReadUserData is available or FS generic read works if mounted?
    // The previous plan said "Load user data file config.json if present".
    // We implemented 'FS_ReadUserData'. Let's use that.
    // Wait, FS_ReadFile reads from mounted Archive OR Dir. 
    // FS_ReadUserData reads from user data dir.
    
    size_t user_size;
    void* user_data = FS_ReadUserData("config.json", &user_size);
    if (user_data) {
        JSValue val = JS_ParseJSON(ctx, (char*)user_data, user_size, "user_config.json");
        if (!JS_IsException(val) && JS_IsObject(val)) {
            // Only allow overrides for window_size and fullscreen per Prompt
            JSValue win = JS_GetPropertyStr(ctx, val, "window_size");
            if (!JS_IsUndefined(win)) {
                int s; 
                if (JS_ToInt32(ctx, &s, win) == 0) g_config.window_scale = s;
            }
            JS_FreeValue(ctx, win);
            
            JSValue fs = JS_GetPropertyStr(ctx, val, "fullscreen");
            if (JS_IsBool(fs)) {
                g_config.fullscreen = JS_ToBool(ctx, fs);
            }
            JS_FreeValue(ctx, fs);
            
            printf("Config: Loaded user overrides.\n");
        }
        JS_FreeValue(ctx, val);
        FS_FreeFile(user_data);
    }
    
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    
    return true;
}

const GameConfig* Config_Get(void) {
    return &g_config;
}
