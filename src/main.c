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
#include "core/config.h"
#include "game/entity.h"
#include "editor/editor.h"
#include "ui/console.h"

// --- Global State for Main Loop ---
static bool running = true;

typedef struct {
    bool forward, backward, left, right;
    bool turn_left, turn_right;
    bool fly_up, fly_down;
    bool sprint;
} InputState;

static InputState input = {false};
static bool editor_has_focus = false;

static TextureID tex_wall;
static TextureID tex_floor;
static TextureID tex_ceil;
static TextureID tex_wood;

// World Data
static Vec2 points[] = {
    {0, 0}, {256, 0}, {256, 64}, {256, 192}, {256, 256}, {0, 256}, // 0-5
    {512, 64}, {512, 192}, // 6-7
    {640, 64}, {640, 192}  // 8-9
};

static Wall walls[] = {
    // Sector 0 (0-5)
    { 0, 1, -1, 0, -1, -1 }, // TexIDs set in Init
    { 1, 2, -1, 0, -1, -1 },
    { 2, 3,  1, -1, 0, 0 }, // P->1
    { 3, 4, -1, 0, -1, -1 },
    { 4, 5, -1, 0, -1, -1 },
    { 5, 0, -1, 0, -1, -1 },
    
    // Sector 1 (6-9)
    { 2, 6, -1, 0, -1, -1 }, // Top wall
    { 6, 7,  2, -1, 0, 0 }, // P->2 (East)
    { 7, 3, -1, 0, -1, -1 }, // Bottom wall
    { 3, 2,  0, -1, 0, 0 }, // P->0 (West)
    
    // Sector 2 (10-13) - Let's make it a small room
    { 6, 8, -1, 0, -1, -1 },
    { 8, 9, -1, 0, -1, -1 },
    { 9, 7, -1, 0, -1, -1 },
    { 7, 6,  1, -1, 0, 0 } // P->1
};

static Sector sectors[] = {
    { 0.0f, 128.0f, 0, 6, 0, 0 }, // S0
    { 0.0f, 128.0f, 6, 4, 0, 0 }, // S1
    { 32.0f, 160.0f, 10, 4, 0, 0 } // S2 (Higher floor)
};

static Map map = {
    .points = points,
    .point_count = 10,
    .walls = walls,
    .wall_count = 14,
    .sectors = sectors,
    .sector_count = 3
};

static GameCamera cam = {
    .pos = {128.0f, 128.0f, 48.0f}, // Start inside Sector 0
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
        Console_Close(); // Auto-hide console
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

    // Console Input
    if (Console_HandleEvent()) {
        if (Console_IsActive()) {
             // Block Game Input
        }
    }

    // Global Toggles
    if (IsKeyPressed(KEY_F12)) {
        // Removed or keep F12 for Editor? Prompt said change it.
        // I will remove it and use F2 below.
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

    if (IsKeyPressed(KEY_F2)) {
        Editor_Toggle();
    }


    // Game Input Handling
    if (!Editor_IsActive() && !Console_IsActive()) {
        input.forward = Config_IsActionDown("move_forward");
        input.backward = Config_IsActionDown("move_backward");
        input.left = Config_IsActionDown("strafe_left");
        input.right = Config_IsActionDown("strafe_right");
        input.turn_left = Config_IsActionDown("turn_left");
        input.turn_right = Config_IsActionDown("turn_right");
        input.fly_up = Config_IsActionDown("fly_up");
        input.fly_down = Config_IsActionDown("fly_down");
        input.sprint = Config_IsActionDown("sprint");
        
        if (Config_IsActionPressed("game_menu")) {
            printf("Game Menu (Stub)\n");
        }
    } else if (Editor_IsActive() && !editor_has_focus) {
         if (Config_IsActionPressed("editor_zoom_in")) printf("Editor Zoom In (Stub)\n");
         if (Config_IsActionPressed("editor_zoom_out")) printf("Editor Zoom Out (Stub)\n");
    } else {
        input = (InputState){0};
    }

    f32 dt = GetFrameTime();
    
    // Cam Update
    f32 move_speed = 192.0f * dt;
    if (input.sprint) move_speed *= 2.5f;
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
    if (input.fly_up) cam.pos.z += move_speed;
    if (input.fly_down) cam.pos.z -= move_speed;
    
    Entity_Update(dt);

    // --- RENDER PIPELINE ---
    Video_BeginFrame();
    
    if (Editor_IsActive()) {
        Editor_Update(&map, &cam);
        
        Rectangle game_rect = GetGameViewRect();
        
        int view = Editor_GetViewMode();
        if (view == 0) {
            Render_Frame(cam, &map);
            Video_DrawGame(&game_rect); 
        } else {
            Render_Map2D(&map, cam, (int)game_rect.x, (int)game_rect.y, (int)game_rect.width, (int)game_rect.height, Editor_GetZoom(), 
                Editor_GetGridSize(),
                Editor_GetSelectedSectorID(), Editor_GetSelectedWallIndex(), 
                Editor_GetHoveredSectorID(), Editor_GetHoveredWallIndex(),
                Editor_GetSelectedEntityID(), Editor_GetHoveredEntityID()); 
 
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

    // 2.5 Run Main Script
    JSValue mainScriptVal = Script_EvalFile("scripts/main.js");
    JS_FreeValue(Script_GetContext(), mainScriptVal);

    // 4. Init Camera
    cam.pos = (Vec3){128.0f, 128.0f, 96.0f};

    // 3. Game Loop
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(Loop, 0, 1);
#else
    while (running && !WindowShouldClose()) {
        Loop();
    }
    
    // Save Config on Exit
    Config_Save();
    
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