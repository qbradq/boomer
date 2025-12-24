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

int main(int argc, char* argv[]) {
    // 1. Initialize Video
    if (!Video_Init("Boomer Engine - Phase 3 Solid")) {
        return 1;
    }
    
    Renderer_Init();

    // 2. Setup Test World
    Wall walls[] = {
        // Sector 0 (Triangle)
        { {1, 1}, {3, 2}, -1 },
        { {3, 2}, {2, 4}, -1 },
        { {2, 4}, {1, 1}, -1 },
        
        // Sector 1 (Square)
        { {4, 1}, {5, 1}, -1 },
        { {5, 1}, {5, 2}, -1 },
        { {5, 2}, {4, 2}, -1 },
        { {4, 2}, {4, 1}, -1 }
    };
    
    Sector sectors[] = {
        { 0.0f, 2.0f, 0, 3 }, // Floor 0, Ceil 2 // Triangle
        { 0.5f, 2.5f, 3, 4 }  // Floor 0.5, Ceil 2.5 // Square
    };
    
    Map map = {
        .walls = walls,
        .wall_count = 7,
        .sectors = sectors,
        .sector_count = 2
    };
    
    Camera cam = {
        .pos = {0.0f, 1.5f, 1.0f}, // Start at 0, 1.5 (Centered Y), height 1
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
