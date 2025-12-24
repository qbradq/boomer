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

static struct nk_context *ctx;
static bool is_active = false;
static int view_mode = 0; // 0 = 3D, 1 = 2D

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
    
    return nk_window_is_any_hovered(ctx);
}

void Editor_Update(void) {
    if (!is_active) return;
    
    // UI Logic defined in Render actually for Immediate Mode usually, 
    // but we can separate if desired. For Nuklear, usually logic happens during command generation.
}

void Editor_Render(void) {
    if (!is_active) return;
    
    // Define UI Layout
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
