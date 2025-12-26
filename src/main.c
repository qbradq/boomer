#include <stdio.h>
#include <stdbool.h>
#include "raylib.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "core/types.h"
#include "video/video.h"
#include "video/texture.h"
#include "render/renderer.h"
#include "world/world_types.h"
#include "world/map_loader.h"
#include "core/fs.h"
#include "core/script_sys.h"
#include "core/config.h"      // Added
#include "game/entity.h"
#include "editor/editor.h"
#include "ui/console.h"       // Added

// JS Binding for Map Loading
static JSValue js_load_map(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char* filename = JS_ToCString(ctx, argv[0]);
    if (!filename) return JS_EXCEPTION;
    
    printf("JS Request: Load Map '%s'\n", filename);
    
    // We need access to the global 'map' variable.
    // Since 'map' is static in main.c, we can access it if we put this function in main.c
    // But we need to forward declare 'map' or move this function after map definition.
    // map is defined below. We'll move this function or use a forward declaration or helper.
    // Let's implement a wrapper `Game_LoadMap` in main? 
    // Or just putting `map` at top scope is cleaner.
    // For now, I will use a forward decl of the helper and implement it after `map` is defined.
    // But this is inside main.c
    
    // Actually, I can just put this binding function AFTER `map` definition.
    // See below chunks.
    return JS_UNDEFINED;
}


typedef struct {
    bool forward, backward, left, right;
    bool turn_left, turn_right;
    bool u, d;
} InputState;

// --- Global State for Main Loop ---
static bool running = true;
static InputState input = {false};
static bool editor_has_focus = false;

static TextureID tex_wall;
static TextureID tex_floor;
static TextureID tex_ceil;
static TextureID tex_wood;

// World Data
static Wall walls[] = {
    // Sector 0 (0-5)
    { {0, 0}, {4, 0}, -1, 0, -1, -1 }, // TexIDs set in Init
    { {4, 0}, {4, 1}, -1, 0, -1, -1 },
    { {4, 1}, {4, 3},  1, -1, 0, 0 }, // P->1
    { {4, 3}, {4, 4}, -1, 0, -1, -1 },
    { {4, 4}, {0, 4}, -1, 0, -1, -1 },
    { {0, 4}, {0, 0}, -1, 0, -1, -1 },
    
    // Sector 1 (6-11)
    { {4, 1}, {8, 1}, -1, 0, -1, -1 }, // Top wall
    { {8, 1}, {8, 3},  2, -1, 0, 0 }, // P->2 (East)
    { {8, 3}, {4, 3}, -1, 0, -1, -1 }, // Bottom wall
    { {4, 3}, {4, 1},  0, -1, 0, 0 }, // P->0 (West)
    
    // Sector 2 (10-13) - Let's make it a small room
    { {8, 1}, {10, 1}, -1, 0, -1, -1 },
    { {10, 1}, {10, 3}, -1, 0, -1, -1 },
    { {10, 3}, {8, 3}, -1, 0, -1, -1 },
    { {8, 3}, {8, 1},  1, -1, 0, 0 } // P->1
};

static Sector sectors[] = {
    { 0.0f, 2.0f, 0, 6, 0, 0 }, // S0
    { 0.0f, 2.0f, 6, 4, 0, 0 }, // S1
    { 0.5f, 2.5f, 10, 4, 0, 0 } // S2 (Higher floor)
};

static Map map = {
    .walls = walls,
    .wall_count = 14,
    .sectors = sectors,
    .sector_count = 3
};

static GameCamera cam = {
    .pos = {2.0f, 2.0f, 0.75f}, // Start inside Sector 0
    .yaw = 0.0f
};

// Moved binding here to access 'map'
static JSValue js_load_map_wrapper(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    const char* filename = JS_ToCString(ctx, argv[0]);
    if (!filename) return JS_EXCEPTION;
    
    bool res = Map_Load(filename, &map);
    
    if (res) {
        Console_SetMapLoaded(true);
        // Reset camera if needed? Or controlled by script?
        // For now, let's just say success.
        printf("Map Loaded via script.\n");
    } else {
        printf("Failed to load map '%s' via script.\n", filename);
    }
    
    JS_FreeCString(ctx, filename);
    return JS_NewBool(ctx, res);
}


// --- Loop Function ---
void Loop(void) {
    if (!running || WindowShouldClose()) {
        running = false;
        #ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
        #endif
        return;
    }

    // Input Poll - Handled by Raylib
    
    // Console Input
    if (Console_HandleEvent()) {
        // If console consumed event (e.g. toggle), we might want to skip other inputs?
        // But Console_HandleEvent currently only handles Toggle.
        // If console is Full/Half, maybe block game input?
        if (Console_IsActive()) {
             // Block Game Input
             // But we still process F-keys?
             // Let's allow F-keys for now.
        }
    }


    // Global Toggles
    if (IsKeyPressed(KEY_F12)) {
        Editor_Toggle();
    }
    
    // Pass to Editor
    if (Editor_IsActive()) {
        Editor_InputBegin();
        if (Editor_HandleInput()) {
            editor_has_focus = true; // Mouse is over UI
        } else {
            editor_has_focus = false;
        }
        Editor_InputEnd();
    }

    if (IsKeyPressed(KEY_F9)) {
        Video_ChangeScale(-1);
    }
    if (IsKeyPressed(KEY_F11)) {
        Video_ChangeScale(1);
    }
    if (IsKeyPressed(KEY_F10)) {
        Video_ToggleFullscreen();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        running = false;
    }
    
    // Game Input Handling
    if (!Editor_IsActive() && !Console_IsActive()) {

        input.forward = IsKeyDown(KEY_W);
        input.backward = IsKeyDown(KEY_S);
        input.left = IsKeyDown(KEY_A); 
        input.right = IsKeyDown(KEY_D); 
        input.turn_left = IsKeyDown(KEY_LEFT);
        input.turn_right = IsKeyDown(KEY_RIGHT);
        input.u = IsKeyDown(KEY_Q);
        input.d = IsKeyDown(KEY_E);
    } else {
            input = (InputState){0};
    }

    f32 dt = GetFrameTime();
    
    // Cam Update
    f32 move_speed = 3.0f * dt;
    f32 rot_speed = 2.0f * dt;
    
    if (input.turn_left) cam.yaw += rot_speed;
    if (input.turn_right) cam.yaw -= rot_speed; 

    f32 cs = cosf(cam.yaw);
    f32 sn = sinf(cam.yaw);
    
    if (input.forward) {
        cam.pos.x += cs * move_speed;
        cam.pos.y += sn * move_speed;
    }
    if (input.backward) {
        cam.pos.x -= cs * move_speed;
        cam.pos.y -= sn * move_speed;
    }
    if (input.left) {
        cam.pos.x -= sn * move_speed;
        cam.pos.y += cs * move_speed;
    }
    if (input.right) {
        cam.pos.x += sn * move_speed;
        cam.pos.y -= cs * move_speed;
    }
    if (input.u) cam.pos.z += move_speed;
    if (input.d) cam.pos.z -= move_speed;
    
    Entity_Update(dt);

    // --- RENDER PIPELINE ---
    Video_BeginFrame();
    
    if (Editor_IsActive()) {
        // ... (Editor rendering kept same)
        Editor_Update(&map, &cam);
        int view = Editor_GetViewMode();
        if (view == 0) {
            Render_Frame(cam, &map);
            Video_DrawGame(NULL); 
        } else {
             int w = GetScreenWidth();
             int h = GetScreenHeight();
             Render_Map2D(&map, cam, 0, 0, w, h, 32.0f, 
                Editor_GetSelectedSectorID(), Editor_GetSelectedWallIndex(), 
                Editor_GetHoveredSectorID(), Editor_GetHoveredWallIndex()); 
        }
        Editor_Render(&map, &cam);
        
    } else {
        // GAME MODE
        Video_Clear((Color){20, 20, 30, 255}); 
        Render_Frame(cam, &map);
        Video_DrawGame(NULL);
    }
    
    // Draw Console (Overlay)
    Console_Update(dt);
    Console_Draw();

    Video_EndFrame();
}


int main(int argc, char** argv) {
    const char* asset_path = "games/demo";
    if (argc > 1) {
        asset_path = argv[1];
    }
    
    // 0. Init FS
    if (!FS_Init(asset_path)) {
        printf("WARNING: Could not mount '%s'\n", asset_path);
    } else {
        printf("FS Mounted: %s\n", asset_path);
    }
    
    // 0.1 Init User Data
    #ifdef __EMSCRIPTEN__
    FS_InitUserData("/data");
    #else
    FS_InitUserData("data");
    #endif
    
    // 0.2 Load Config
    Config_Load();

    // 0.5 Init Script System
    if (!Script_Init()) {
        printf("CRITICAL: Failed to init Script System.\n");
        return 1;
    }
    Script_RegisterFunc("loadMap", js_load_map_wrapper, 1);
    
    // 0.6 Init Entity System
    Entity_Init();

    // 1. Initialize Video
    if (!Video_Init("Boomer Engine")) {
        return 1;
    }
    
    // Init Console (After Video)
    Console_Init();
    
    // Init Editor
    Editor_Init();
    
    Renderer_Init();
    Texture_Init();

    // 2. Load Map (Via Script now)
    // if( !Map_Load("test.json", &map) ) ... REMOVED hardcoded load

    // 2.5 Run Main Script
    JSValue mainScriptVal = Script_EvalFile("scripts/main.js");
    JS_FreeValue(Script_GetContext(), mainScriptVal);

    // 4. Init Camera
    cam.pos = (Vec3){2.0f, 2.0f, 1.5f};

    // 3. Game Loop
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(Loop, 0, 1);
#else
    while (running && !WindowShouldClose()) {
        Loop();
    }
    
    Console_Shutdown();
    Editor_Shutdown();
    Video_Shutdown();
    Texture_Shutdown();
    Entity_Shutdown();
    Script_Shutdown();
    FS_Shutdown();
#endif


    return 0;
}
