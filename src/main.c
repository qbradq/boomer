#include <stdio.h>
#include <stdbool.h>
#include "SDL.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "core/types.h"
#include "core/math_utils.h"
#include "video/video.h"
#include "video/texture.h"
#include "render/renderer.h"
#include "world/world_types.h"
#include "core/fs.h"
#include <stdio.h> // For logging FS test

#include "core/lua_sys.h"
#include "game/entity.h"

typedef struct {
    bool forward, backward, left, right;
    bool turn_left, turn_right;
    bool u, d;
} InputState;

int main(int argc, char** argv) {
    const char* asset_path = "games/demo";
    if (argc > 1) {
        asset_path = argv[1];
    }
    
    // 0. Init FS
    if (!FS_Init(asset_path)) {
        printf("WARNING: Could not mount '%s'\n", asset_path);
    } else {
        size_t sz;
        char* data = FS_ReadFile("test.txt", &sz);
        if (data) {
            printf("FS TEST: Read 'test.txt' (%zu bytes): %s\n", sz, data);
            FS_FreeFile(data);
        } else {
            printf("FS TEST: Could not find 'test.txt'\n");
        }
    }
    
    // 0.5 Init Lua
    if (!Lua_Init()) {
        printf("CRITICAL: Failed to init Lua.\n");
        return 1;
    }
    
    // 0.6 Init Entity System
    Entity_Init();

    // 1. Initialize Video
    if (!Video_Init("Boomer Engine - Phase 4 Portals")) {
        return 1;
    }
    
    Renderer_Init();
    Texture_Init();

    // Load Textures
    TextureID tex_wall = Texture_Load("textures/grid_blue.png");

    // 2. Setup Test World (2 Connected Sectors)
    Wall walls[] = {
        // Sector 0
        { {0, 0}, {4, 0}, -1, tex_wall },
        { {4, 0}, {4, 1}, -1, tex_wall },
        { {4, 1}, {4, 3},  1, -1 }, // Portal
        { {4, 3}, {4, 4}, -1, tex_wall },
        { {4, 4}, {0, 4}, -1, tex_wall },
        { {0, 4}, {0, 0}, -1, tex_wall },
        
        // Sector 1
        { {4, 1}, {6, 1}, -1, tex_wall },
        { {6, 1}, {6, 3}, -1, tex_wall },
        { {6, 3}, {4, 3}, -1, tex_wall },
        { {4, 3}, {4, 1},  0, -1 }  // Portal
    };
    
    Sector sectors[] = {
        { 0.0f, 2.0f, 0, 6 }, // Sector 0
        { 0.0f, 2.0f, 6, 4 }  // Sector 1
    };
    
    Map map = {
        .walls = walls,
        .wall_count = 10,
        .sectors = sectors,
        .sector_count = 2
    };
    
    Camera cam = {
        .pos = {2.0f, 2.0f, 0.75f}, // Start inside Sector 0
        .yaw = 0.0f
    };
    
    // Spawn Test Entity
    printf("Spawning Test Entity...\n");
    Entity_Spawn("scripts/test_ent.lua", (Vec3){3.0f, 3.0f, 1.0f});

    // 3. Game Loop
    bool running = true;
    InputState input = {0};
    u32 last_time = SDL_GetTicks();

    while (running) {
        // Input Poll
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            // The original code had an SDLK_ESCAPE check here, but the new snippet removes it.
            // If it's desired, it should be re-added. For now, following the snippet.
        }
        
        // Input Handling
        const u8* keys = SDL_GetKeyboardState(NULL);
        input.forward = keys[SDL_SCANCODE_W];
        input.backward = keys[SDL_SCANCODE_S];
        input.left = keys[SDL_SCANCODE_A]; // Strafe
        input.right = keys[SDL_SCANCODE_D]; // Strafe
        input.turn_left = keys[SDL_SCANCODE_LEFT];
        input.turn_right = keys[SDL_SCANCODE_RIGHT];
        input.u = keys[SDL_SCANCODE_Q];
        input.d = keys[SDL_SCANCODE_E];

        u32 current_time = SDL_GetTicks();
        f32 dt = (current_time - last_time) / 1000.0f;
        last_time = current_time;
        
        // Camera Update
        f32 move_speed = 3.0f * dt;
        f32 rot_speed = 2.0f * dt;
        
        if (input.turn_left) cam.yaw += rot_speed; // PLUS for Left (Standard CCW)
        if (input.turn_right) cam.yaw -= rot_speed; // MINUS for Right

        f32 cs = cosf(cam.yaw);
        f32 sn = sinf(cam.yaw);
        
        // Forward/Back
        if (input.forward) {
            cam.pos.x += cs * move_speed;
            cam.pos.y += sn * move_speed;
        }
        if (input.backward) {
            cam.pos.x -= cs * move_speed;
            cam.pos.y -= sn * move_speed;
        }
        
        // Strafe
        if (input.left) {
            cam.pos.x -= sn * move_speed; // -sin, +cos for left vector (-90 deg)
            cam.pos.y += cs * move_speed;
        }
        if (input.right) {
            cam.pos.x += sn * move_speed;
            cam.pos.y -= cs * move_speed;
        }
        
        // Fly
        if (input.u) cam.pos.z += move_speed;
        if (input.d) cam.pos.z -= move_speed;

        // Entity Update
        Entity_Update(dt);

        // Render
        Video_Clear((Color){20, 20, 30, 255}); // Dark blue background
        
        Render_Frame(cam, &map);

        Video_Present();
    }

    Video_Shutdown();
    Texture_Shutdown();
    Lua_Shutdown();
    FS_Shutdown();
    return 0;
}
