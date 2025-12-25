#include <stdio.h>
#include <stdbool.h>
#include "SDL.h"


#include "core/types.h"
#include "core/math_utils.h"
#include "video/video.h"
#include "video/texture.h"
#include "render/renderer.h"
#include "world/world_types.h"
#include "core/fs.h"
#include "core/script_sys.h"
#include "game/entity.h"
#include "editor/editor.h"

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
        printf("FS Mounted: %s\n", asset_path);
    }
    
    // 0.5 Init Script System
    if (!Script_Init()) {
        printf("CRITICAL: Failed to init Script System.\n");
        return 1;
    }
    
    // 0.6 Init Entity System
    Entity_Init();

    // 1. Initialize Video
    if (!Video_Init("Boomer Engine - Phase 11 Editor")) {
        return 1;
    }
    
    // Init Editor
    Editor_Init(Video_GetWindow(), Video_GetRenderer());
    
    Renderer_Init();
    Texture_Init();

    // Load Textures
    TextureID tex_wall = Texture_Load("textures/modern/inner_wall_1.png");
    TextureID tex_floor = Texture_Load("textures/grid_blue.png");
    TextureID tex_ceil = Texture_Load("textures/grid_blue.png");
    TextureID tex_wood = Texture_Load("textures/modern/wood_0.png");

    // 2. Setup Test World (3 Connected Sectors)
    Wall walls[] = {
        // Sector 0 (0-5)
        { {0, 0}, {4, 0}, -1, tex_wall, -1, -1 },
        { {4, 0}, {4, 1}, -1, tex_wall, -1, -1 },
        { {4, 1}, {4, 3},  1, -1, tex_wood, tex_wood }, // P->1
        { {4, 3}, {4, 4}, -1, tex_wall, -1, -1 },
        { {4, 4}, {0, 4}, -1, tex_wall, -1, -1 },
        { {0, 4}, {0, 0}, -1, tex_wall, -1, -1 },
        
        // Sector 1 (6-11)
        { {4, 1}, {8, 1}, -1, tex_wall, -1, -1 }, // Top wall
        { {8, 1}, {8, 3},  2, -1, tex_wood, tex_wood }, // P->2 (East)
        { {8, 3}, {4, 3}, -1, tex_wall, -1, -1 }, // Bottom wall
        { {4, 3}, {4, 1},  0, -1, tex_wood, tex_wood }, // P->0 (West)
        
        // Sector 2 (10-13) - Let's make it a small room
        { {8, 1}, {10, 1}, -1, tex_wall, -1, -1 },
        { {10, 1}, {10, 3}, -1, tex_wall, -1, -1 },
        { {10, 3}, {8, 3}, -1, tex_wall, -1, -1 },
        { {8, 3}, {8, 1},  1, -1, tex_wood, tex_wood } // P->1
    };
    
    Sector sectors[] = {
        { 0.0f, 2.0f, 0, 6, tex_floor, tex_ceil }, // S0
        { 0.0f, 2.0f, 6, 4, tex_floor, tex_ceil }, // S1
        { 0.5f, 2.5f, 10, 4, tex_floor, tex_ceil } // S2 (Higher floor)
    };
    
    Map map = {
        .walls = walls,
        .wall_count = 14,
        .sectors = sectors,
        .sector_count = 3
    };
    
    Camera cam = {
        .pos = {2.0f, 2.0f, 0.75f}, // Start inside Sector 0
        .yaw = 0.0f
    };
    
    // Spawn Test Entity
    printf("Spawning Test Entity...\n");
    Entity_Spawn("scripts/test_ent.js", (Vec3){3.0f, 3.0f, 1.0f});

    // 3. Game Loop
    bool running = true;
    InputState input = {0};
    u32 last_time = SDL_GetTicks();
    bool editor_has_focus = false;

    while (running) {
        // Capture Editor State at start of frame to ensure consistent Input Begin/End pairing
        bool editor_was_active = Editor_IsActive();

        // Input Poll
        if (editor_was_active) Editor_InputBegin();
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
             // Global Toggles
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_F12) {
                    Editor_Toggle();
                }
            }
            
            // Pass to Editor (if it was active at start of frame)
            // Even if we toggle OFF during this frame, we finish the input frame for the editor.
            if (editor_was_active) {
                if (Editor_HandleEvent(&event)) {
                    editor_has_focus = true; // Mouse is over UI
                } else {
                    editor_has_focus = false;
                }
            }

            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_F9:
                        Video_ChangeScale(-1);
                        break;
                    case SDLK_F11:
                        Video_ChangeScale(1);
                        break;
                    case SDLK_F10:
                        Video_ToggleFullscreen();
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                }
            }
        }
        
        if (editor_was_active) Editor_InputEnd();
        
        // Game Input Handling
        if (!Editor_IsActive()) {
            const u8* keys = SDL_GetKeyboardState(NULL);
            input.forward = keys[SDL_SCANCODE_W];
            input.backward = keys[SDL_SCANCODE_S];
            input.left = keys[SDL_SCANCODE_A]; 
            input.right = keys[SDL_SCANCODE_D]; 
            input.turn_left = keys[SDL_SCANCODE_LEFT];
            input.turn_right = keys[SDL_SCANCODE_RIGHT];
            input.u = keys[SDL_SCANCODE_Q];
            input.d = keys[SDL_SCANCODE_E];
        } else {
             input = (InputState){0};
        }

        u32 current_time = SDL_GetTicks();
        f32 dt = (current_time - last_time) / 1000.0f;
        last_time = current_time;
        
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
            // Update Editor Logic (Selection)
            Editor_Update(&map, &cam);
            
            // EDITOR MODE
            int view = Editor_GetViewMode();
            if (view == 0) {
                // 3D View (in a window? For now fill screen behind UI)
                Render_Frame(cam, &map);
                Video_DrawGame(NULL); // Fill
            } else {
                // 2D View
                int w, h;
                SDL_GetWindowSize(Video_GetWindow(), &w, &h);
                Render_Map2D(Video_GetRenderer(), &map, cam, 0, 0, w, h, 32.0f, Editor_GetSelectedSectorID(), Editor_GetSelectedWallIndex(), Editor_GetHoveredSectorID(), Editor_GetHoveredWallIndex()); 
            }
            
            // Draw UI
            Editor_Render(&map, &cam);
            
        } else {
            // GAME MODE
            Video_Clear((Color){20, 20, 30, 255}); 
            Render_Frame(cam, &map);
            Video_DrawGame(NULL);
        }

        Video_EndFrame();
    }
    
    Editor_Shutdown();
    Video_Shutdown();
    Texture_Shutdown();
    Entity_Shutdown();
    Script_Shutdown();
    FS_Shutdown();
    return 0;
}
