#include "renderer.h"
#include "../video/video.h"
#include "../video/texture.h"
#include "../core/math_utils.h"
#include <math.h>

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define FOV_H DEG2RAD(90.0f)
#define NEAR_Z 0.1f
#define MAX_RECURSION 16

void Renderer_Init(void) {
    // any Pre-calc tables here
}

// Transform World Position to Camera Relative (Rotated & Translated)
// P_cam = Rot(-Yaw) * (P_world - Cam_pos)
static Vec3 TransformToCamera(Vec3 p, Camera cam) {
    Vec3 local = vec3_sub(p, cam.pos);
    f32 cs = cosf(-cam.yaw);
    f32 sn = sinf(-cam.yaw);
    
    return (Vec3){
        local.x * cs - local.y * sn,
        -(local.x * sn + local.y * cs), // Negate Y to map Left(+Y) -> camera Left(-Y) -> Screen Left
        local.z
    };
}

bool WorldToScreen(Vec3 world_pos, Camera cam, Vec2* screen_out) {
    Vec3 p = TransformToCamera(world_pos, cam);
    
    if (p.x < NEAR_Z) return false;
    
    f32 scale = (VIDEO_WIDTH / 2.0f) / tanf(FOV_H / 2.0f);
    
    screen_out->x = (VIDEO_WIDTH / 2.0f) + (p.y / p.x) * scale;
    screen_out->y = (VIDEO_HEIGHT / 2.0f) - (p.z / p.x) * scale;
    
    return true;
}

// Clip a wall segment against the Near Plane (p.x > NEAR_Z)
// Returns t1, t2 (0.0 to 1.0) relative to original segment
static bool ClipWall(Vec3 p1, Vec3 p2, Vec3* out1, Vec3* out2, f32* t1, f32* t2) {
    *out1 = p1;
    *out2 = p2;
    *t1 = 0.0f;
    *t2 = 1.0f;
    
    f32 n = NEAR_Z;
    
    if (p1.x < n && p2.x < n) return false;
    if (p1.x >= n && p2.x >= n) return true;
    
    // t = (n - x1) / (x2 - x1)
    f32 t = (n - p1.x) / (p2.x - p1.x);
    Vec3 intersect = vec3_lerp(p1, p2, t);
    intersect.x = n;
    
    if (p1.x < n) {
        *out1 = intersect;
        *t1 = t;
    } else {
        *out2 = intersect;
        *t2 = t;
    }
    
    return true;
}

// Helper to draw Floor/Ceiling Span
static void DrawFlat(int x, int y1, int y2, f32 height_diff, Camera cam, Texture* tex) {
    if (y1 > y2) return;
    if (!tex) {
        Video_DrawVertLine(x, y1, y2, (Color){50, 50, 50, 255}); // Gray fallback
        return;
    }

    f32 cx = VIDEO_WIDTH / 2.0f;
    f32 cy = VIDEO_HEIGHT / 2.0f;
    f32 scale = (VIDEO_WIDTH / 2.0f) / tanf(FOV_H / 2.0f);

    // Ray Direction for this column
    // Screen X -> normalized -1..1 -> view space dir
    f32 screen_x_norm = (x - cx) / (VIDEO_WIDTH / 2.0f); // -1 to 1 (approx)
    
    // Actually, accurate ray dir:
    // view_x = (x - cx) / scale
    // view_y = 1.0 (depth)
    // view_z = ... (handled by loop)
    
    // Rotated Ray Dir
    f32 view_x = (x - cx) / scale;
    f32 cs = cosf(cam.yaw);
    f32 sn = sinf(cam.yaw);

    // Correct Ray Dir derivation:
    // ... (comments)
    
    f32 rdx = cs + view_x * sn;
    f32 rdy = sn - view_x * cs;
    
    // Correct for Fisheye?
    // Z = height / pixel_y
    // Distance = Z / cos(angle)?
    // Standard Raycasting formula handles distance correct if we use planar Z.
    
    for (int y = y1; y <= y2; ++y) {
        if (y == (int)cy) continue; // Singularity
        
        f32 z = height_diff * scale / (f32)(y - cy);
        if (z < 0) z = -z; // Distance is positive
        
        // World Pos
        f32 wx = cam.pos.x + rdx * z;
        f32 wy = cam.pos.y + rdy * z;
        
        // Texture Coords (1 unit = 64 pixels)
        int tx = (int)(wx * 64.0f) % tex->width;
        int ty = (int)(wy * 64.0f) % tex->height;
        if (tx < 0) tx += tex->width;
        if (ty < 0) ty += tex->height;
        
        u32 color = tex->pixels[ty * tex->width + tx];
        video_pixels[y * VIDEO_WIDTH + x] = color;
    }
}

// Recursive Sector Render with Y-Clipping
static void RenderSector(Map* map, Camera cam, SectorID sector_id, int min_x, int max_x, i16* y_top, i16* y_bot, int depth) {
    if (depth > MAX_RECURSION) return;
    if (min_x >= max_x) return;

    Sector* sector = &map->sectors[sector_id];
    f32 scale = (VIDEO_WIDTH / 2.0f) / tanf(FOV_H / 2.0f);
    f32 center_x = VIDEO_WIDTH / 2.0f;
    f32 center_y = VIDEO_HEIGHT / 2.0f;
    
    Texture* floor_tex = Texture_Get(sector->floor_tex_id);
    Texture* ceil_tex = Texture_Get(sector->ceil_tex_id);

    for (u32 w = 0; w < sector->num_walls; ++w) {
        WallID wid = sector->first_wall + w;
        Wall* wall = &map->walls[wid];
        
        // 1. Transform & Clip
        Vec3 p1_world = { wall->p2.x, wall->p2.y, 0 }; 
        Vec3 p2_world = { wall->p1.x, wall->p1.y, 0 };
        
        f32 dx = p2_world.x - p1_world.x;
        f32 dy = p2_world.y - p1_world.y;
        f32 wall_len = sqrtf(dx*dx + dy*dy);
        
        Vec3 p1_cam = TransformToCamera(p1_world, cam);
        Vec3 p2_cam = TransformToCamera(p2_world, cam);
        
        Vec3 c1, c2;
        f32 t1_clip, t2_clip;
        if (!ClipWall(p1_cam, p2_cam, &c1, &c2, &t1_clip, &t2_clip)) continue;
        
        // 2. Project X
        f32 x1 = center_x + (c1.y / c1.x) * scale;
        f32 x2 = center_x + (c2.y / c2.x) * scale;
        
        if (x1 >= x2) continue; // Cull
        
        // 4. Clip to Window
        int ix1 = (int)ceilf(x1);
        int ix2 = (int)ceilf(x2);
        
        int draw_x1 = (ix1 < min_x) ? min_x : ix1;
        int draw_x2 = (ix2 > max_x) ? max_x : ix2;
        
        if (draw_x1 >= draw_x2) continue;
        
        // 5. Calculate Heights
        f32 ceil_h = sector->ceil_height - cam.pos.z;
        f32 floor_h = sector->floor_height - cam.pos.z;
        
        f32 y1a = center_y - (ceil_h / c1.x) * scale;
        f32 y1b = center_y - (floor_h / c1.x) * scale;
        f32 y2a = center_y - (ceil_h / c2.x) * scale;
        f32 y2b = center_y - (floor_h / c2.x) * scale;
        
        f32 iz1 = 1.0f / c1.x;
        f32 iz2 = 1.0f / c2.x;
        
        f32 u_scale = wall_len * 64.0f;
        f32 u1 = t1_clip * u_scale;
        f32 u2 = t2_clip * u_scale;
        
        f32 uz1 = u1 * iz1;
        f32 uz2 = u2 * iz2;
        
        // Preparing next recursion buffers if portal
        i16 next_y_top[VIDEO_WIDTH];
        i16 next_y_bot[VIDEO_WIDTH];
        bool portal = (wall->next_sector != -1);
        
        // 6. Draw Columns
        for (int x = draw_x1; x < draw_x2; ++x) {
            f32 t_screen = (x - x1) / (x2 - x1);
            
            f32 y_ceil_f = y1a + (y2a - y1a) * t_screen;
            f32 y_floor_f = y1b + (y2b - y1b) * t_screen;
            
            int y_ceil = (int)y_ceil_f;
            int y_floor = (int)y_floor_f;
            
            int cy_top = y_top[x];
            int cy_bot = y_bot[x];
            
            // Draw Ceiling (from top clip to wall top)
            if (y_ceil > cy_top) {
                DrawFlat(x, cy_top, min(y_ceil, cy_bot), sector->ceil_height - cam.pos.z, cam, ceil_tex);
            }
            // Draw Floor (from wall bottom to bot clip)
            if (y_floor < cy_bot) {
                DrawFlat(x, max(y_floor, cy_top), cy_bot, cam.pos.z - sector->floor_height, cam, floor_tex);
            }
            
            // Wall / Portal Window
            Texture* top_tex = Texture_Get(wall->top_texture_id);
            Texture* bot_tex = Texture_Get(wall->bottom_texture_id);
            Texture* wall_tex = Texture_Get(wall->texture_id);

            int wy_top = cy_top;
            int wy_bot = cy_bot;
            
            if (portal) {
                Sector* next_s = &map->sectors[wall->next_sector];
                f32 n_ceil_h = next_s->ceil_height - cam.pos.z;
                f32 n_floor_h = next_s->floor_height - cam.pos.z;
                
                f32 ny1a = center_y - (n_ceil_h / c1.x) * scale;
                f32 ny1b = center_y - (n_floor_h / c1.x) * scale;
                f32 ny2a = center_y - (n_ceil_h / c2.x) * scale;
                f32 ny2b = center_y - (n_floor_h / c2.x) * scale;
                
                f32 ny_ceil_f = ny1a + (ny2a - ny1a) * t_screen;
                f32 ny_floor_f = ny1b + (ny2b - ny1b) * t_screen;
                
                int ny_ceil = (int)ny_ceil_f;
                int ny_floor = (int)ny_floor_f;
                
                // --- Upper Wall (Transom) ---
                int u_start = max(y_ceil, cy_top);
                int u_end = min(ny_ceil, cy_bot);
                
                if (u_start < u_end) {
                     if (top_tex) {
                        f32 iz = iz1 + (iz2 - iz1) * t_screen;
                        f32 uz = uz1 + (uz2 - uz1) * t_screen;
                        int tex_x = (int)(uz / iz);
                        
                        f32 world_h = (sector->ceil_height - next_s->ceil_height);
                        f32 v_scale = world_h * 64.0f;
                        float pixel_h = ny_ceil_f - y_ceil_f;
                        float v_s = v_scale / pixel_h;
                        
                        Video_DrawTexturedColumn(x, u_start, u_end - 1, top_tex, tex_x, (u_start - y_ceil_f) * v_s, v_s);
                     } else {
                         Video_DrawVertLine(x, u_start, u_end - 1, (Color){80, 80, 80, 255});
                     }
                }
                
                // --- Lower Wall (Step) ---
                int b_start = max(ny_floor, cy_top);
                int b_end = min(y_floor, cy_bot);
                
                if (b_start < b_end) {
                    if (bot_tex) {
                        f32 iz = iz1 + (iz2 - iz1) * t_screen;
                        f32 uz = uz1 + (uz2 - uz1) * t_screen;
                        int tex_x = (int)(uz / iz);
                        
                        f32 world_h = (next_s->floor_height - sector->floor_height);
                        f32 v_scale = world_h * 64.0f;
                        float pixel_h = y_floor_f - ny_floor_f;
                        float v_s = v_scale / pixel_h;
                        
                        Video_DrawTexturedColumn(x, b_start, b_end - 1, bot_tex, tex_x, (b_start - ny_floor_f) * v_s, v_s);
                    } else {
                        Video_DrawVertLine(x, b_start, b_end - 1, (Color){80, 80, 80, 255});
                    }
                }
                
                // Update Window for Recursion
                wy_top = max(ny_ceil, cy_top);
                wy_bot = min(ny_floor, cy_bot);
                
            } else {
                // Not a portal - Draw Solid Wall
                int w_start = max(y_ceil, cy_top);
                int w_end = min(y_floor, cy_bot);
                
                if (w_start < w_end) {
                    if (wall_tex) {
                        f32 iz = iz1 + (iz2 - iz1) * t_screen;
                        f32 uz = uz1 + (uz2 - uz1) * t_screen;
                        int tex_x = (int)(uz / iz);
                        
                        f32 world_height = sector->ceil_height - sector->floor_height;
                        float v_scale = world_height * 64.0f;
                        float height = y_floor_f - y_ceil_f;
                        float v_step = v_scale / height;
                        
                        Video_DrawTexturedColumn(x, w_start, w_end - 1, wall_tex, tex_x, (w_start - y_ceil_f) * v_step, v_step);
                    } else {
                        Video_DrawVertLine(x, w_start, w_end - 1, (Color){100, 100, 100, 255});
                    }
                }
            }
            
            if (wy_top < wy_bot) {
                if (portal) {
                    next_y_top[x] = wy_top;
                    next_y_bot[x] = wy_bot;
                }
            } else {
                if (portal) {
                   next_y_top[x] = VIDEO_HEIGHT;
                   next_y_bot[x] = -1;
                }
            }
        }
        
        if (portal) {
            RenderSector(map, cam, wall->next_sector, draw_x1, draw_x2, next_y_top, next_y_bot, depth + 1);
        }
    }
}

void Render_Frame(Camera cam, Map* map) {
    SectorID start_sector = GetSectorAt(map, (Vec2){cam.pos.x, cam.pos.y});
    if (start_sector == -1) start_sector = 0; 
    
    Video_Clear((Color){20, 20, 30, 255});
    
    // Init Clipping Buffers
    i16 y_top[VIDEO_WIDTH];
    i16 y_bot[VIDEO_WIDTH];
    for(int i=0; i<VIDEO_WIDTH; ++i) {
        y_top[i] = 0;
        y_bot[i] = VIDEO_HEIGHT - 1;
    }
    
    RenderSector(map, cam, start_sector, 0, VIDEO_WIDTH, y_top, y_bot, 0);
}

void Render_Map2D(struct SDL_Renderer* ren, Map* map, Camera cam, int x, int y, int w, int h, float zoom, int highlight_sector, int highlight_wall_index, int hovered_sector, int hovered_wall_index) {
    // Set viewport/clip
    SDL_Rect view = {x, y, w, h};
    SDL_RenderSetViewport(ren, &view);
    
    // Background
    SDL_SetRenderDrawColor(ren, 30, 30, 40, 255);
    SDL_RenderFillRect(ren, NULL); // Fills viewport
    
    // Center is (w/2, h/2) representing CameraPos
    float cx = w / 2.0f;
    float cy = h / 2.0f;
    
    // Draw Grid (Optional)
    SDL_SetRenderDrawColor(ren, 50, 50, 60, 255);
    int grid_size = (int)(zoom);
    if (grid_size > 4) {
        int off_x = (int)(cx - cam.pos.x * zoom) % grid_size;
        int off_y = (int)(cy + cam.pos.y * zoom) % grid_size;
        
        for (int gx = off_x; gx < w; gx += grid_size) SDL_RenderDrawLine(ren, gx, 0, gx, h);
        for (int gy = off_y; gy < h; gy += grid_size) SDL_RenderDrawLine(ren, 0, gy, w, gy);
    }
    
    // Draw Walls
    for (int i = 0; i < map->wall_count; ++i) {
        Wall* wall = &map->walls[i];
        
        float x1 = cx + (wall->p1.x - cam.pos.x) * zoom;
        float y1 = cy - (wall->p1.y - cam.pos.y) * zoom;
        float x2 = cx + (wall->p2.x - cam.pos.x) * zoom;
        float y2 = cy - (wall->p2.y - cam.pos.y) * zoom;
        
        if (wall->next_sector != -1) {
            SDL_SetRenderDrawColor(ren, 200, 50, 50, 255); 
        } else {
            SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        }
        SDL_RenderDrawLineF(ren, x1, y1, x2, y2);
    }
    
    // Render Highlighted/Hovered Sector
    if (hovered_sector != -1 && hovered_sector != highlight_sector && hovered_sector < map->sector_count) {
         Sector* s = &map->sectors[hovered_sector];
         SDL_SetRenderDrawColor(ren, 0, 255, 0, 255); // Lime Green
         
         for (u32 i = 0; i < s->num_walls; ++i) {
            Wall* wall = &map->walls[s->first_wall + i];
            float x1 = cx + (wall->p1.x - cam.pos.x) * zoom;
            float y1 = cy - (wall->p1.y - cam.pos.y) * zoom;
            float x2 = cx + (wall->p2.x - cam.pos.x) * zoom;
            float y2 = cy - (wall->p2.y - cam.pos.y) * zoom;
            
            SDL_RenderDrawLineF(ren, x1, y1, x2, y2);
            
            // Normals (Inward: dy, -dx)
             float nx = (y2 - y1);
             float ny = -(x2 - x1);
             float len = sqrtf(nx*nx + ny*ny);
             if (len > 0) {
                 nx /= len; ny /= len;
                 SDL_RenderDrawLineF(ren, (x1+x2)/2, (y1+y2)/2, (x1+x2)/2 + nx*8, (y1+y2)/2 + ny*8);
             }
             
             // Vertices
             SDL_FRect r = {x1 - 2, y1 - 2, 5, 5}; // 5x5
             SDL_RenderFillRectF(ren, &r);
         }
    }
    
    if (highlight_sector != -1 && highlight_sector < map->sector_count) {
        Sector* s = &map->sectors[highlight_sector];
        
        for (u32 i = 0; i < s->num_walls; ++i) {
            Wall* wall = &map->walls[s->first_wall + i];
            float x1 = cx + (wall->p1.x - cam.pos.x) * zoom;
            float y1 = cy - (wall->p1.y - cam.pos.y) * zoom;
            float x2 = cx + (wall->p2.x - cam.pos.x) * zoom;
            float y2 = cy - (wall->p2.y - cam.pos.y) * zoom;
            
            // Highlight Logic for Walls
            if ((int)i == highlight_wall_index) {
                SDL_SetRenderDrawColor(ren, 0, 255, 255, 255); // Cyan
            } 
            else if ((int)i == hovered_wall_index) {
                SDL_SetRenderDrawColor(ren, 0, 255, 0, 255); // Lime Green
            }
            else {
                SDL_SetRenderDrawColor(ren, 255, 255, 0, 255); // Yellow for Sector
            }
            
            // Draw Line
            SDL_RenderDrawLineF(ren, x1, y1, x2, y2);
            
            // Draw Tick Normal (Inward)
             float nx = (y2 - y1);
             float ny = -(x2 - x1);
             float len = sqrtf(nx*nx + ny*ny);
             if (len > 0) {
                 nx /= len; ny /= len;
                 SDL_RenderDrawLineF(ren, (x1+x2)/2, (y1+y2)/2, (x1+x2)/2 + nx*8, (y1+y2)/2 + ny*8);
             }
             
             // Vertices (5x5)
             Color v_col = {255, 255, 0, 255}; // Default Sector Yellow
             if ((int)i == highlight_wall_index) v_col = (Color){0, 255, 255, 255}; // Cyan
             else if ((int)i == hovered_wall_index) v_col = (Color){0, 255, 0, 255}; // Lime Green
             
             SDL_SetRenderDrawColor(ren, v_col.r, v_col.g, v_col.b, v_col.a);
             SDL_FRect r = {x1 - 2, y1 - 2, 5, 5};
             SDL_RenderFillRectF(ren, &r);
        }
    }
    
    // Draw Camera
    SDL_SetRenderDrawColor(ren, 0, 255, 0, 255);
    SDL_FRect player = {cx - 3, cy - 3, 6, 6};
    SDL_RenderFillRectF(ren, &player);
    
    // Frustum
    float dir_len = 20.0f;
    float cs = cosf(cam.yaw); 
    float sn = sinf(cam.yaw);
    SDL_RenderDrawLineF(ren, cx, cy, cx + cs * dir_len, cy - sn * dir_len);
    
    SDL_RenderSetViewport(ren, NULL);
}
