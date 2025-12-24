#include <stdio.h>
#include <stdbool.h>
#include "SDL.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "core/types.h"
#include "core/math_utils.h"
#include "video/video.h"
#include "render/renderer.h"
#include "world/world_types.h"
#include "core/fs.h"
#include <stdio.h> // For logging FS test

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

    // 1. Initialize Video
    if (!Video_Init("Boomer Engine - Phase 3 Solid")) {
        return 1;
    }
    
    Renderer_Init();

    // 2. Setup Test World (2 Connected Sectors)
    // Sector 0 (Big Room) - 6 Walls
    // Sector 1 (Small Room) - 4 Walls
    
    static Wall walls[] = {
        // Sector 0
        { {0, 0}, {4, 0}, -1 },
        { {4, 0}, {4, 1}, -1 },
        { {4, 1}, {4, 3},  1 }, // Portal to Sector 1
        { {4, 3}, {4, 4}, -1 },
        { {4, 4}, {0, 4}, -1 },
        { {0, 4}, {0, 0}, -1 },
        
        // Sector 1
        { {4, 1}, {6, 1}, -1 },
        { {6, 1}, {6, 3}, -1 },
        { {6, 3}, {4, 3}, -1 },
        { {4, 3}, {4, 1},  0 }  // Portal to Sector 0
    };
    
    static Sector sectors[] = {
        { 0.0f, 3.0f, 0, 6 }, // Sector 0: 6 walls starting at 0
        { 0.5f, 2.5f, 6, 4 }  // Sector 1: 4 walls starting at 6
    };
    
    Map map = {
        .walls = walls,
        .wall_count = 10,
        .sectors = sectors,
        .sector_count = 2
    };
    
    Camera cam = {
        .pos = {2.0f, 2.0f, 1.5f}, // Start inside Sector 0
        .yaw = 0.0f
    };

    // 3. Game Loop
    bool running = true;
    SDL_Event event;
    const u8* keystate = SDL_GetKeyboardState(NULL);

    while (running) {
        // Input Poll
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }
        
        // Input Continuous
        f32 move_speed = 0.05f;
        f32 rot_speed = 0.03f;
        
        if (keystate[SDL_SCANCODE_LEFT])  cam.yaw += rot_speed;
        if (keystate[SDL_SCANCODE_RIGHT]) cam.yaw -= rot_speed;
        
        // Move relative to Yaw
        f32 cs = cosf(cam.yaw);
        f32 sn = sinf(cam.yaw);
        
        // W/S Forward/Back
        if (keystate[SDL_SCANCODE_W]) {
            cam.pos.x += cs * move_speed;
            cam.pos.y += sn * move_speed;
        }
        if (keystate[SDL_SCANCODE_S]) {
            cam.pos.x -= cs * move_speed;
            cam.pos.y -= sn * move_speed;
        }
        
        // A/D Strafe Left/Right
        if (keystate[SDL_SCANCODE_A]) {
            cam.pos.x -= sn * move_speed;
            cam.pos.y += cs * move_speed;
        }
        if (keystate[SDL_SCANCODE_D]) {
            cam.pos.x += sn * move_speed;
            cam.pos.y -= cs * move_speed;
        }
        
        // Q/E Up/Down
        if (keystate[SDL_SCANCODE_Q]) cam.pos.z += move_speed;
        if (keystate[SDL_SCANCODE_E]) cam.pos.z -= move_speed;


        // Render
        Video_Clear((Color){20, 20, 30, 255}); // Dark blue background
        
        Render_Frame(cam, &map);

        Video_Present();
        SDL_Delay(16); // ~60 FPS cap
    }

    Video_Shutdown();
    return 0;
}
