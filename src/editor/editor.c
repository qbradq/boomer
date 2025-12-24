#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_renderer.h"

#include "editor.h"
#include "../video/video.h" // For VIDEO_WIDTH/HEIGHT info if needed

#include "editor.h"
#include "../video/video.h" 
#include "../video/texture.h"
#include "../world/world_types.h"
#include "../render/renderer.h" // For GetSectorAt (if exposed) or copy prototype
// GetSectorAt is in renderer.c but not header? It is in renderer.c static?
// Wait, Render_Frame calls GetSectorAt. It's likely static in renderer.c?
// StartLine 373 of renderer.c says GetSectorAt is called.
// I need `GetSectorAt` exposed in `renderer.h` or `world.h`.
// Let's assume I need to expose it first.
// Actually, `GetSectorAt` implementation works on World Space.
// Let's just create a helper here or modify renderer.h first?
// Modifying renderer.h to expose GetSectorAt is cleaner.
// I'll assume it's exposed for this replacement, and do another tool call for renderer.h.

static struct nk_context *ctx;
static bool is_active = false;
static int view_mode = 0; // 0 = 3D, 1 = 2D
static int selected_sector = -1;

void Editor_Init(SDL_Window* window, SDL_Renderer* renderer) {
    // Initialize Nuklear with SDL Renderer
    ctx = nk_sdl_init(window, renderer);
    
    // Load default font
    struct nk_font_atlas *atlas;
    nk_sdl_font_stash_begin(&atlas);
    // nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14, 0);
    nk_sdl_font_stash_end();
    
    printf("Editor Initialized.\n");
}

bool Editor_HandleEvent(SDL_Event* event) {
    if (!is_active) return false;
    
    nk_input_begin(ctx);
    nk_sdl_handle_event(event);
    nk_input_end(ctx);
    
    // Capture input if hovering UI
    if (nk_window_is_any_hovered(ctx)) return true;
    
    // Also capture input if we are editing text?
    return false;
}

void Editor_Update(struct Map* map, struct Camera* cam) {
    if (!is_active) return;
    
    // Selection Logic (Click in 2D View)
    if (view_mode == 1) { // 2D View
        // Check for Mouse Click (Left Button)
        // SDL Mouse State? Or Nuklear Input?
        // Since we want to interact with "World", we handle it here if Nuklear didn't swallow it.
        // But `HandleEvent` returns true if hovered.
        // So here we check if Mouse Down and NOT hovered?
        // Or check standard SDL mouse state inside main loop? 
        // Better: Check SDL mouse state here.
        
        int mx, my;
        u32 buttons = SDL_GetMouseState(&mx, &my);
        static bool was_down = false;
        bool is_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT));
        
        if (is_down && !was_down) {
            if (!nk_window_is_any_hovered(ctx)) {
                // Determine World Pos
                int w, h;
                SDL_GetWindowSize(Video_GetWindow(), &w, &h);
                float cx = w / 2.0f;
                float cy = h / 2.0f;
                float zoom = 32.0f; // Must match Render_Map2D
                
                // Screen X -> World X
                // x_screen = cx + (bx - cam.x) * zoom
                // (x_screen - cx)/zoom = bx - cam.x
                // bx = (x_screen - cx)/zoom + cam.x
                
                float world_x = (mx - cx) / zoom + cam->pos.x;
                
                // Screen Y -> World Y
                // y_screen = cy - (by - cam.y) * zoom
                // -(y_screen - cy)/zoom = by - cam.y
                // by = cam.y - (y_screen - cy)/zoom
                
                float world_y = cam->pos.y - (my - cy) / zoom;
                
                // Raycast/Point Check
                // We don't have GetSectorAt exposed yet.
                // Assuming it will be: `SectorID GetSectorAt(Map* map, Vec2 pos)`
                selected_sector = GetSectorAt(map, (Vec2){world_x, world_y});
                printf("Selected Sector: %d (at %.2f, %.2f)\n", selected_sector, world_x, world_y);
            }
        }
        was_down = is_down;
    }
}

void Editor_Render(struct Map* map, struct Camera* cam) {
    if (!is_active) return;
    
    // Toolbar
    if (nk_begin(ctx, "Editor Toolbar", nk_rect(0, 0, 200, 600),
        NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
        NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {
        
        nk_layout_row_dynamic(ctx, 30, 1);
        nk_label(ctx, "View Mode:", NK_TEXT_LEFT);
        if (nk_option_label(ctx, "3D Game View", view_mode == 0)) view_mode = 0;
        if (nk_option_label(ctx, "2D Map View", view_mode == 1)) view_mode = 1;

        nk_layout_row_dynamic(ctx, 30, 1);
        if (nk_button_label(ctx, "Exit Editor")) {
            Editor_Toggle();
        }
        
        if (selected_sector != -1) {
             nk_layout_row_dynamic(ctx, 30, 1);
             nk_label(ctx, "Sector Inspector:", NK_TEXT_LEFT);
             char buf[64];
             snprintf(buf, 64, "Sector #%d", selected_sector);
             nk_label(ctx, buf, NK_TEXT_LEFT);
             
             Sector* s = &map->sectors[selected_sector];
             
             // Heights
             nk_property_float(ctx, "Floor H", -10.0f, &s->floor_height, 20.0f, 0.5f, 0.1f);
             nk_property_float(ctx, "Ceil H", -10.0f, &s->ceil_height, 20.0f, 0.5f, 0.1f);
             
             // Textures
             nk_layout_row_dynamic(ctx, 30, 1);
             nk_label(ctx, "Floor Tex:", NK_TEXT_LEFT);
             nk_label(ctx, Texture_GetName(s->floor_tex_id), NK_TEXT_LEFT);
             nk_property_int(ctx, "#FloorID", -1, &s->floor_tex_id, 20, 1, 0.5f);
             
             nk_label(ctx, "Ceil Tex:", NK_TEXT_LEFT);
             nk_label(ctx, Texture_GetName(s->ceil_tex_id), NK_TEXT_LEFT);
             nk_property_int(ctx, "#CeilID", -1, &s->ceil_tex_id, 20, 1, 0.5f);
        }
    }
    nk_end(ctx);
    
    nk_sdl_render(NK_ANTI_ALIASING_ON);
}

void Editor_Shutdown(void) {
    nk_sdl_shutdown();
}

bool Editor_IsActive(void) {
    return is_active;
}

void Editor_Toggle(void) {
    is_active = !is_active;
    printf("Editor Mode: %s\n", is_active ? "ON" : "OFF");
}

int Editor_GetViewMode(void) {
    return view_mode;
}

int Editor_GetSelectedSectorID(void) {
    return selected_sector;
}
