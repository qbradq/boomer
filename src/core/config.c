#include "config.h"
#include "fs.h"
#include "../core/script_sys.h" // Access to QuickJS headers
#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

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

// --- Input System Data ---
#define MAX_BINDINGS 32
#define MAX_KEYS_PER_BINDING 4

typedef struct {
    char* action;
    int keys[MAX_KEYS_PER_BINDING];
    int key_count;
} KeyBinding;

static KeyBinding bindings[MAX_BINDINGS];
static int binding_count = 0;

// Mapping string <-> keycode
typedef struct {
    const char* name;
    int key;
} KeyNameMap;

static const KeyNameMap key_names[] = {
    {"UNKNOWN", 0},
    {"SPACE", KEY_SPACE},
    {"ESCAPE", KEY_ESCAPE},
    {"ENTER", KEY_ENTER},
    {"TAB", KEY_TAB},
    {"BACKSPACE", KEY_BACKSPACE},
    {"INSERT", KEY_INSERT},
    {"DELETE", KEY_DELETE},
    {"RIGHT", KEY_RIGHT},
    {"LEFT", KEY_LEFT},
    {"DOWN", KEY_DOWN},
    {"UP", KEY_UP},
    {"PAGE_UP", KEY_PAGE_UP},
    {"PAGE_DOWN", KEY_PAGE_DOWN},
    {"HOME", KEY_HOME},
    {"END", KEY_END},
    {"CAPS_LOCK", KEY_CAPS_LOCK},
    {"SCROLL_LOCK", KEY_SCROLL_LOCK},
    {"NUM_LOCK", KEY_NUM_LOCK},
    {"PRINT_SCREEN", KEY_PRINT_SCREEN},
    {"PAUSE", KEY_PAUSE},
    {"F1", KEY_F1}, {"F2", KEY_F2}, {"F3", KEY_F3}, {"F4", KEY_F4},
    {"F5", KEY_F5}, {"F6", KEY_F6}, {"F7", KEY_F7}, {"F8", KEY_F8},
    {"F9", KEY_F9}, {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12},
    {"LEFT_SHIFT", KEY_LEFT_SHIFT}, {"LEFT_CONTROL", KEY_LEFT_CONTROL},
    {"LEFT_ALT", KEY_LEFT_ALT}, {"LEFT_SUPER", KEY_LEFT_SUPER},
    {"RIGHT_SHIFT", KEY_RIGHT_SHIFT}, {"RIGHT_CONTROL", KEY_RIGHT_CONTROL},
    {"RIGHT_ALT", KEY_RIGHT_ALT}, {"RIGHT_SUPER", KEY_RIGHT_SUPER},
    {"KB_MENU", KEY_KB_MENU},
    {"LEFT_BRACKET", KEY_LEFT_BRACKET},
    {"BACKSLASH", KEY_BACKSLASH},
    {"RIGHT_BRACKET", KEY_RIGHT_BRACKET},
    {"GRAVE", KEY_GRAVE},
    {"KP_0", KEY_KP_0}, {"KP_1", KEY_KP_1}, {"KP_2", KEY_KP_2},
    {"KP_3", KEY_KP_3}, {"KP_4", KEY_KP_4}, {"KP_5", KEY_KP_5},
    {"KP_6", KEY_KP_6}, {"KP_7", KEY_KP_7}, {"KP_8", KEY_KP_8},
    {"KP_9", KEY_KP_9},
    {"KP_DECIMAL", KEY_KP_DECIMAL}, {"KP_DIVIDE", KEY_KP_DIVIDE},
    {"KP_MULTIPLY", KEY_KP_MULTIPLY}, {"KP_SUBTRACT", KEY_KP_SUBTRACT},
    {"KP_ADD", KEY_KP_ADD}, {"KP_ENTER", KEY_KP_ENTER}, {"KP_EQUAL", KEY_KP_EQUAL},
    {"APOSTROPHE", KEY_APOSTROPHE}, {"COMMA", KEY_COMMA},
    {"MINUS", KEY_MINUS}, {"PERIOD", KEY_PERIOD},
    {"SLASH", KEY_SLASH}, {"SEMICOLON", KEY_SEMICOLON},
    {"EQUAL", KEY_EQUAL},
    {"A", KEY_A}, {"B", KEY_B}, {"C", KEY_C}, {"D", KEY_D},
    {"E", KEY_E}, {"F", KEY_F}, {"G", KEY_G}, {"H", KEY_H},
    {"I", KEY_I}, {"J", KEY_J}, {"K", KEY_K}, {"L", KEY_L},
    {"M", KEY_M}, {"N", KEY_N}, {"O", KEY_O}, {"P", KEY_P},
    {"Q", KEY_Q}, {"R", KEY_R}, {"S", KEY_S}, {"T", KEY_T},
    {"U", KEY_U}, {"V", KEY_V}, {"W", KEY_W}, {"X", KEY_X},
    {"Y", KEY_Y}, {"Z", KEY_Z},
    {"0", KEY_ZERO}, {"1", KEY_ONE}, {"2", KEY_TWO}, {"3", KEY_THREE},
    {"4", KEY_FOUR}, {"5", KEY_FIVE}, {"6", KEY_SIX}, {"7", KEY_SEVEN},
    {"8", KEY_EIGHT}, {"9", KEY_NINE},
    {NULL, 0}
};

static int GetKeyByName(const char* name) {
    if (!name) return 0;
    for (int i = 0; key_names[i].name != NULL; ++i) {
        if (strcasecmp(key_names[i].name, name) == 0) {
            return key_names[i].key;
        }
    }
    return 0;
}

static const char* GetNameByKey(int key) {
    for (int i = 0; key_names[i].name != NULL; ++i) {
        if (key_names[i].key == key) {
            return key_names[i].name;
        }
    }
    return "UNKNOWN";
}

static KeyBinding* FindBinding(const char* action) {
    for (int i = 0; i < binding_count; ++i) {
        if (strcmp(bindings[i].action, action) == 0) {
            return &bindings[i];
        }
    }
    return NULL;
}

static void AddBinding(const char* action, int key) {
    KeyBinding* b = FindBinding(action);
    if (b) {
        // Add key to existing binding if space allows
        if (b->key_count < MAX_KEYS_PER_BINDING) {
            // Check duplicate
            for(int i=0; i<b->key_count; ++i) if(b->keys[i] == key) return;
            b->keys[b->key_count++] = key;
        }
    } else {
        if (binding_count < MAX_BINDINGS) {
            bindings[binding_count].action = strdup(action);
            bindings[binding_count].keys[0] = key;
            bindings[binding_count].key_count = 1;
            binding_count++;
        }
    }
}

static void ClearBindings(void) {
    for (int i = 0; i < binding_count; ++i) {
        free(bindings[i].action);
    }
    binding_count = 0;
}

static void SetDefaultBindings(void) {
    ClearBindings();
    AddBinding("move_forward", KEY_W);
    AddBinding("move_forward", KEY_UP);
    
    AddBinding("move_backward", KEY_S);
    AddBinding("move_backward", KEY_DOWN);
    
    AddBinding("strafe_left", KEY_A);
    AddBinding("strafe_right", KEY_D);
    
    AddBinding("turn_left", KEY_LEFT);
    AddBinding("turn_right", KEY_RIGHT);
    
    AddBinding("fly_up", KEY_SPACE);
    AddBinding("fly_down", KEY_LEFT_CONTROL);
    
    AddBinding("sprint", KEY_LEFT_SHIFT);
    AddBinding("toggle_console", KEY_GRAVE);
    
    AddBinding("game_menu", KEY_ESCAPE);
    
    AddBinding("editor_zoom_in", KEY_EQUAL);
    AddBinding("editor_zoom_out", KEY_MINUS);
}

// --- Helper Functions ---

static u32 ParseColor(const char* hex_str) {
    if (!hex_str || hex_str[0] != '#') return 0;
    unsigned int r, g, b, a = 255;
    int len = strlen(hex_str);
    if (len == 7) {
        sscanf(hex_str + 1, "%02x%02x%02x", &r, &g, &b);
    } else if (len == 9) {
        sscanf(hex_str + 1, "%02x%02x%02x%02x", &r, &g, &b, &a);
    } else {
        return 0xFFFFFFFF;
    }
    return (r << 24) | (g << 16) | (b << 8) | a;
}

static char* ColorToHex(u32 col) {
    static char buf[10];
    snprintf(buf, 10, "#%02X%02X%02X%02X", 
        (col >> 24) & 0xFF, (col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF);
    return buf;
}

// --- Loading Logic ---

static void LoadConfigObj(JSContext* ctx, JSValue obj) {
    JSValue res = JS_GetPropertyStr(ctx, obj, "logical_resolution");
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
    
    // Inputs
    JSValue inputs = JS_GetPropertyStr(ctx, obj, "input");
    if (JS_IsObject(inputs)) {
        JSPropertyEnum *tab;
        uint32_t len;
        if (JS_GetOwnPropertyNames(ctx, &tab, &len, inputs, JS_GPN_STRING_MASK) != -1) {
            ClearBindings(); // Clear defaults if input object exists
            for(int i = 0; i < len; i++) {
                const char* action = JS_AtomToCString(ctx, tab[i].atom);
                JSValue val = JS_GetProperty(ctx, inputs, tab[i].atom);
                
                if (JS_IsString(val)) {
                    const char* k = JS_ToCString(ctx, val);
                    AddBinding(action, GetKeyByName(k));
                    JS_FreeCString(ctx, k);
                } else if (JS_IsArray(val)) {
                    JSValue lenVal = JS_GetPropertyStr(ctx, val, "length");
                    int arrLen;
                    JS_ToInt32(ctx, &arrLen, lenVal);
                    JS_FreeValue(ctx, lenVal);
                    
                    for(int j=0; j<arrLen; ++j) {
                        JSValue v = JS_GetPropertyUint32(ctx, val, j);
                        if (JS_IsString(v)) {
                            const char* k = JS_ToCString(ctx, v);
                            AddBinding(action, GetKeyByName(k));
                            JS_FreeCString(ctx, k);
                        }
                        JS_FreeValue(ctx, v);
                    }
                }
                
                JS_FreeValue(ctx, val);
                JS_FreeCString(ctx, action);
            }
            js_free(ctx, tab);
        }
    }
    JS_FreeValue(ctx, inputs);
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

bool Config_Load(void) {
    SetDefaultBindings();

    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);
    if (!ctx) return false;
    
    // 1. Base Config
    LoadJSONFile(ctx, "config.json");
    
    // 2. User Config
    size_t user_size;
    void* user_data = FS_ReadUserData("config.json", &user_size);
    if (user_data) {
        JSValue val = JS_ParseJSON(ctx, (char*)user_data, user_size, "user_config.json");
        if (!JS_IsException(val) && JS_IsObject(val)) {
            LoadConfigObj(ctx, val);
            printf("Config: Loaded user overrides.\n");
        }
        JS_FreeValue(ctx, val);
        FS_FreeFile(user_data);
    }
    
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    
    return true;
}

// --- Saving Logic ---

void Config_Save(void) {
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);
    
    JSValue obj = JS_NewObject(ctx);
    
    // Window
    JS_SetPropertyStr(ctx, obj, "window_size", JS_NewInt32(ctx, g_config.window_scale));
    JS_SetPropertyStr(ctx, obj, "fullscreen", JS_NewBool(ctx, g_config.fullscreen));
    
    // Input
    JSValue inp = JS_NewObject(ctx);
    for (int i = 0; i < binding_count; ++i) {
        KeyBinding* b = &bindings[i];
        if (b->key_count == 1) {
             JS_SetPropertyStr(ctx, inp, b->action, JS_NewString(ctx, GetNameByKey(b->keys[0])));
        } else {
            JSValue arr = JS_NewArray(ctx);
            for(int j=0; j<b->key_count; ++j) {
                JS_SetPropertyUint32(ctx, arr, j, JS_NewString(ctx, GetNameByKey(b->keys[j])));
            }
            JS_SetPropertyStr(ctx, inp, b->action, arr);
        }
    }
    JS_SetPropertyStr(ctx, obj, "input", inp);
    
    JSValue json = JS_JSONStringify(ctx, obj, JS_UNDEFINED, JS_NewInt32(ctx, 4));
    const char* str = JS_ToCString(ctx, json);
    
    FS_WriteUserData("config.json", str, strlen(str));
    
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, json);
    JS_FreeValue(ctx, obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    
    printf("Config: Saved to user data.\n");
}

const GameConfig* Config_Get(void) {
    return &g_config;
}

bool Config_IsActionDown(const char* action) {
    KeyBinding* b = FindBinding(action);
    if (!b) return false;
    for (int i=0; i < b->key_count; ++i) {
        if (IsKeyDown(b->keys[i])) return true;
    }
    return false;
}

bool Config_IsActionPressed(const char* action) {
    KeyBinding* b = FindBinding(action);
    if (!b) return false;
    for (int i=0; i < b->key_count; ++i) {
        if (IsKeyPressed(b->keys[i])) return true;
    }
    return false;
}