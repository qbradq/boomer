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
#include "../video/video.h" 
#include "../video/texture.h"
#include "../world/world_types.h"
#include "../render/renderer.h"

static struct nk_context *ctx;
static bool is_active = false;
static int view_mode = 0; // 0 = 3D, 1 = 2D
static int selected_sector = -1;
static int selected_wall_index = -1;
static int hovered_wall_index = -1;
static int hovered_sector_id = -1;

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

void Editor_InputBegin(void) {
    if (!is_active) return;
    nk_input_begin(ctx);
}

void Editor_InputEnd(void) {
    if (!is_active) return;
    nk_input_end(ctx);
}

bool Editor_HandleEvent(SDL_Event* event) {
    if (!is_active) return false;
    
    nk_sdl_handle_event(event);
    
    if (nk_window_is_any_hovered(ctx)) {
        return true; 
    }
    
    return false;
}

void Editor_Update(struct Map* map, struct Camera* cam) {
    if (!is_active) return;
    
    hovered_wall_index = -1;
    hovered_sector_id = -1;
    
    // Selection Logic (Click in 2D View)
    if (view_mode == 1) { // 2D View
        int mx, my;
        u32 buttons = SDL_GetMouseState(&mx, &my);
        static bool was_down = false;
        bool is_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT));
        
        // Determine World Pos
        int w, h;
        SDL_GetWindowSize(Video_GetWindow(), &w, &h);
        float cx = w / 2.0f;
        float cy = h / 2.0f;
        float zoom = 32.0f; // Must match Render_Map2D
        
        float world_x = (mx - cx) / zoom + cam->pos.x;
        float world_y = cam->pos.y - (my - cy) / zoom;
        
        // 1. Always Calc Hovered Sector
        hovered_sector_id = GetSectorAt(map, (Vec2){world_x, world_y});
        
        // 2. Calc Wall Hover (Only if Sector Selected)
        if (selected_sector != -1) {
             Sector* s = &map->sectors[selected_sector];
             float threshold = 10.0f / zoom; 
             
             for (u32 i = 0; i < s->num_walls; ++i) {
                 Wall* wall = &map->walls[s->first_wall + i];
                 
                 // Dist Squared Point to Segment
                 float l2 = (wall->p2.x-wall->p1.x)*(wall->p2.x-wall->p1.x) + (wall->p2.y-wall->p1.y)*(wall->p2.y-wall->p1.y);
                 if (l2 == 0) continue;
                 float t = ((world_x - wall->p1.x) * (wall->p2.x - wall->p1.x) + (world_y - wall->p1.y) * (wall->p2.y - wall->p1.y)) / l2;
                 if (t < 0) t = 0;
                 if (t > 1) t = 1;
                 
                 float px = wall->p1.x + t * (wall->p2.x - wall->p1.x);
                 float py = wall->p1.y + t * (wall->p2.y - wall->p1.y);
                 float dist_sq = (world_x - px)*(world_x - px) + (world_y - py)*(world_y - py);
                 
                 if (dist_sq < threshold * threshold) {
                     hovered_wall_index = i;
                     break; 
                 }
             }
        }
        
        // 3. Click Logic
        if (is_down && !was_down) {
            if (!nk_window_is_any_hovered(ctx)) {
                
                // Priority: Select Wall if hovering one
                if (hovered_wall_index != -1) {
                    selected_wall_index = hovered_wall_index;
                } 
                else if (hovered_sector_id == selected_sector) {
                    // Clicked empty space in selected sector -> Deselect Wall
                    selected_wall_index = -1;
                } 
                else {
                    // Clicked a different sector (or void)
                    selected_sector = hovered_sector_id;
                    selected_wall_index = -1; // New sector, reset wall
                }
                
                printf("Selected Sector: %d, Wall: %d (Hover Sec: %d)\n", selected_sector, selected_wall_index, hovered_sector_id);
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
        
        // Toolbar Row
        nk_layout_row_static(ctx, 30, 30, 2);
        
        // 1. View Toggle
        if (view_mode == 0) {
            if (nk_button_symbol(ctx, NK_SYMBOL_RECT_SOLID)) {
                view_mode = 1;
            }
        } else {
            if (nk_button_symbol(ctx, NK_SYMBOL_TRIANGLE_RIGHT)) {
                view_mode = 0;
            }
        }
        
        // 2. Exit Editor
        if (nk_button_symbol(ctx, NK_SYMBOL_X)) {
             Editor_Toggle();
        }
        
        // Determine what to show in Inspector
        int inspect_sector = selected_sector;
        if (inspect_sector == -1) inspect_sector = hovered_sector_id;
        
        if (inspect_sector != -1) {
             nk_layout_row_dynamic(ctx, 30, 1);
             nk_label(ctx, "Sector Inspector:", NK_TEXT_LEFT);
             char buf[64];
             const char* status = (selected_sector == inspect_sector) ? "Selected" : "Hovered";
             snprintf(buf, 64, "Sector #%d (%s)", inspect_sector, status);
             nk_label(ctx, buf, NK_TEXT_LEFT);
             
             Sector* s = &map->sectors[inspect_sector];
             
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
             
             // Wall Inspector
             // Show Selected Wall (if in this sector) OR Hovered Wall (if not selected)
             int inspect_wall = -1;
             
             if (selected_sector == inspect_sector && selected_wall_index != -1) {
                 inspect_wall = selected_wall_index;
             } else if (hovered_wall_index != -1 && selected_sector == inspect_sector) {
                 // Only show hovered wall if we are inspecting the selected sector
                 inspect_wall = hovered_wall_index;
             }
             
             if (inspect_wall != -1 && inspect_wall < (int)s->num_walls) {
                 Wall* w = &map->walls[s->first_wall + inspect_wall];
                 
                 nk_layout_row_dynamic(ctx, 20, 1);
                 nk_label(ctx, "----------------", NK_TEXT_CENTERED);
                 
                 char wall_buf[64];
                 const char* w_status = (selected_wall_index == inspect_wall) ? "Selected" : "Hovered";
                 snprintf(wall_buf, 64, "Wall #%d (%s)", inspect_wall, w_status);
                 nk_label(ctx, wall_buf, NK_TEXT_LEFT);
                 
                 if (w->next_sector == -1) {
                     nk_label(ctx, "Type: SOLID", NK_TEXT_LEFT);
                     
                     nk_label(ctx, "Main Tex:", NK_TEXT_LEFT);
                     nk_label(ctx, Texture_GetName(w->texture_id), NK_TEXT_LEFT);
                     nk_property_int(ctx, "#WallTex", -1, &w->texture_id, 20, 1, 0.5f);
                 } else {
                     char portal_buf[64];
                     snprintf(portal_buf, 64, "Type: PORTAL -> Sec %d", w->next_sector);
                     nk_label(ctx, portal_buf, NK_TEXT_LEFT);
                     
                     nk_label(ctx, "Mid Tex (Mask):", NK_TEXT_LEFT);
                     nk_label(ctx, Texture_GetName(w->texture_id), NK_TEXT_LEFT);
                     nk_property_int(ctx, "#WallMid", -1, &w->texture_id, 20, 1, 0.5f);
                     
                     nk_label(ctx, "Top Tex:", NK_TEXT_LEFT);
                     nk_label(ctx, Texture_GetName(w->top_texture_id), NK_TEXT_LEFT);
                     nk_property_int(ctx, "#WallTop", -1, &w->top_texture_id, 20, 1, 0.5f);
                     
                     nk_label(ctx, "Bot Tex:", NK_TEXT_LEFT);
                     nk_label(ctx, Texture_GetName(w->bottom_texture_id), NK_TEXT_LEFT);
                     nk_property_int(ctx, "#WallBot", -1, &w->bottom_texture_id, 20, 1, 0.5f);
                 }
             }
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
    if (is_active) {
        // Flush input state
        nk_input_begin(ctx);
        nk_input_end(ctx);
    }
    printf("Editor Mode: %s\n", is_active ? "ON" : "OFF");
}

int Editor_GetViewMode(void) {
    return view_mode;
}

int Editor_GetSelectedSectorID(void) {
    return selected_sector;
}

int Editor_GetSelectedWallIndex(void) {
    return selected_wall_index;
}

int Editor_GetHoveredSectorID(void) {
    return hovered_sector_id;
}

int Editor_GetHoveredWallIndex(void) {
    return hovered_wall_index;
}
