#include "renderer.h"
#include "../video/video.h"
#include "../video/texture.h"
#include "../core/math_utils.h"
#include "raylib.h"
#include <math.h>
#include "../game/entity.h"

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define FOV_H (90.0f * DEG2RAD)
#define NEAR_Z 0.1f
#define MAX_RECURSION 16

void Renderer_Init(void) {
    // any Pre-calc tables here
}

// Transform World Position to Camera Relative (Rotated & Translated)
// P_cam = Rot(-Yaw) * (P_world - Cam_pos)
static Vec3 TransformToCamera(Vec3 p, GameCamera cam) {
    Vec3 local = vec3_sub(p, cam.pos);
    f32 cs = cosf(-cam.yaw);
    f32 sn = sinf(-cam.yaw);
    
    return (Vec3){
        local.x * cs - local.y * sn,
        -(local.x * sn + local.y * cs), // Negate Y to map Left(+Y) -> camera Left(-Y) -> Screen Left
        local.z
    };
}

bool WorldToScreen(Vec3 world_pos, GameCamera cam, Vec2* screen_out) {
    Vec3 p = TransformToCamera(world_pos, cam);
    
    if (p.x < NEAR_Z) return false;
    
    f32 scale = (VIDEO_WIDTH / 2.0f) / tanf(FOV_H / 2.0f);
    
    screen_out->x = (VIDEO_WIDTH / 2.0f) + (p.y / p.x) * scale;
    screen_out->y = (VIDEO_HEIGHT / 2.0f) - (p.z / p.x) * scale;
    
    return true;
}

// Clip a wall segment against the Near Plane (p.x > near_z)
// Returns t1, t2 (0.0 to 1.0) relative to original segment
static bool ClipWall(Vec3 p1, Vec3 p2, Vec3* out1, Vec3* out2, f32* t1, f32* t2, f32 near_z) {
    *out1 = p1;
    *out2 = p2;
    *t1 = 0.0f;
    *t2 = 1.0f;
    
    f32 n = near_z;
    
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
static void DrawFlat(int x, int y1, int y2, f32 height_diff, GameCamera cam, GameTexture* tex) {
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
        
        // Texture Coords (1 unit = 1 pixel)
        int tx = (int)(wx) % tex->width;
        int ty = (int)(wy) % tex->height;
        if (tx < 0) tx += tex->width;
        if (ty < 0) ty += tex->height;
        
        u32 color = tex->pixels[ty * tex->width + tx];
        video_pixels[y * VIDEO_WIDTH + x] = color;
    }
}

// Recursive Sector Render with Y-Clipping
static void RenderSector(Map* map, GameCamera cam, SectorID sector_id, int min_x, int max_x, i16* y_top, i16* y_bot, int depth) {
    if (depth > MAX_RECURSION) return;
    if (min_x >= max_x) return;

    Sector* sector = &map->sectors[sector_id];
    f32 scale = (VIDEO_WIDTH / 2.0f) / tanf(FOV_H / 2.0f);
    f32 center_x = VIDEO_WIDTH / 2.0f;
    f32 center_y = VIDEO_HEIGHT / 2.0f;
    
    GameTexture* floor_tex = Texture_Get(sector->floor_tex_id);
    GameTexture* ceil_tex = Texture_Get(sector->ceil_tex_id);

    for (u32 w = 0; w < sector->num_walls; ++w) {
        WallID wid = sector->first_wall + w;
        Wall* wall = &map->walls[wid];
        
        // Lookup vertex coordinates
        Vec2 p1 = map->points[wall->p1];
        Vec2 p2 = map->points[wall->p2];
        
        // 1. Transform & Clip
        Vec3 p1_world = { p2.x, p2.y, 0 }; // Swapped for winding order? Or just p2->p1 convention?
        Vec3 p2_world = { p1.x, p1.y, 0 }; // Usually sectors are CW or CCW. 
        // Original code had p2.x, p2.y then p1.x, p1.y. Let's maintain that.
        // It implies the wall goes from p1 to p2, but rendering iterates differently or transforms require it?
        // Wait, normally transforming a segment (x1,y1) -> (x2,y2).
        // If the original code swapped them, I will keep swapping them unless I see a reason not to.
        // Original: p1_world = { wall->p2.x ... }; p2_world = { wall->p1.x ... }
        // So yes, swapped.
        
        f32 dx = p2_world.x - p1_world.x;
        f32 dy = p2_world.y - p1_world.y;
        f32 wall_len = sqrtf(dx*dx + dy*dy);
        
        Vec3 p1_cam = TransformToCamera(p1_world, cam);
        Vec3 p2_cam = TransformToCamera(p2_world, cam);
        
        Vec3 c1, c2;
        f32 t1_clip, t2_clip;
        bool portal = (wall->next_sector != -1);
        f32 clip_dist = portal ? 0.005f : NEAR_Z; // Use closer clip for portals to prevent blinking

        if (!ClipWall(p1_cam, p2_cam, &c1, &c2, &t1_clip, &t2_clip, clip_dist)) continue;
        
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
        
        f32 u_scale = wall_len;
        f32 u1 = t1_clip * u_scale;
        f32 u2 = t2_clip * u_scale;
        
        f32 uz1 = u1 * iz1;
        f32 uz2 = u2 * iz2;
        
        // Preparing next recursion buffers if portal
        // Preparing next recursion buffers if portal
        i16 next_y_top[MAX_VIDEO_WIDTH];
        i16 next_y_bot[MAX_VIDEO_WIDTH];
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
            GameTexture* top_tex = Texture_Get(wall->top_texture_id);
            GameTexture* bot_tex = Texture_Get(wall->bottom_texture_id);
            GameTexture* wall_tex = Texture_Get(wall->texture_id);

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
                        f32 v_scale = world_h;
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
                        f32 v_scale = world_h;
                        float pixel_h = y_floor_f - ny_floor_f;
                        float v_s = v_scale / pixel_h;
                        
                        Video_DrawTexturedColumn(x, b_start, b_end - 1, bot_tex, tex_x, (b_start - ny_floor_f) * v_s, v_s);
                    } else {
                        Video_DrawVertLine(x, b_start, b_end - 1, (Color){80, 80, 80, 255});
                    }
                }
                
                // Update Window for Recursion
                // Clip against Current Sector Ceiling/Floor as well
                wy_top = max(ny_ceil, max(y_ceil, cy_top));
                wy_bot = min(ny_floor, min(y_floor, cy_bot));
                
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
                        float v_scale = world_height;
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

void Render_Frame(GameCamera cam, Map* map) {
    SectorID start_sector = GetSectorAt(map, (Vec2){cam.pos.x, cam.pos.y});
    if (start_sector == -1) start_sector = 0; 
    
    Video_Clear((Color){20, 20, 30, 255});
    
    // Init Clipping Buffers
    // Init Clipping Buffers
    i16 y_top[MAX_VIDEO_WIDTH];
    i16 y_bot[MAX_VIDEO_WIDTH];
    for(int i=0; i<VIDEO_WIDTH; ++i) {
        y_top[i] = 0;
        y_bot[i] = VIDEO_HEIGHT - 1;
    }
    
    RenderSector(map, cam, start_sector, 0, VIDEO_WIDTH, y_top, y_bot, 0);
}

void Render_Map2D(Map* map, GameCamera cam, Vec2 view_pos, int x, int y, int w, int h, float zoom, int grid_size, int highlight_sector, int highlight_wall_index, int hovered_sector, int hovered_wall_index, int selected_entity_id, int hovered_entity_id, int hovered_point_index, int selected_point_index) {
    // Set viewport/clip
    BeginScissorMode(x, y, w, h);
    
    // 1. Clear background to black
    DrawRectangle(x, y, w, h, BLACK);
    
    float cx = x + w / 2.0f;
    float cy = y + h / 2.0f;
    
    // 2. Draw Grid
    // Use passed grid_size, adjusted by zoom
    float visual_grid_step = (float)grid_size * zoom;
    
    // If grid gets too dense, double spacing until it's reasonable
    while (visual_grid_step < 4.0f) {
        visual_grid_step *= 2.0f;
    }
    
    float off_x = fmod(view_pos.x * zoom, visual_grid_step);
    float off_y = fmod(view_pos.y * zoom, visual_grid_step);
    
    // Draw Grid (25% brightness - 64/255)
    Color grid_col = (Color){64, 64, 64, 255};
    
    // Vertical lines
    for (float gx = cx - off_x; gx < x + w; gx += visual_grid_step) DrawLine(gx, y, gx, y + h, grid_col);
    for (float gx = cx - off_x - visual_grid_step; gx > x; gx -= visual_grid_step) DrawLine(gx, y, gx, y + h, grid_col);
    
    // Horizontal lines
    for (float gy = cy + off_y; gy < y + h; gy += visual_grid_step) DrawLine(x, gy, x + w, gy, grid_col);
    for (float gy = cy + off_y - visual_grid_step; gy > y; gy -= visual_grid_step) DrawLine(x, gy, x + w, gy, grid_col);
    
    // 3. Draw Walls
    for (int i = 0; i < map->wall_count; ++i) {
        Wall* wall = &map->walls[i];
        Vec2 p1 = map->points[wall->p1];
        Vec2 p2 = map->points[wall->p2];
        
        float x1 = cx + (p1.x - view_pos.x) * zoom;
        float y1 = cy - (p1.y - view_pos.y) * zoom; // Invert Y
        float x2 = cx + (p2.x - view_pos.x) * zoom;
        float y2 = cy - (p2.y - view_pos.y) * zoom;
        
        bool is_portal = (wall->next_sector != -1);
        Color wall_col = is_portal ? RED : WHITE;
        
        // Draw Line
        DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 1.0f, wall_col);
        
        // Draw Points (Squares 5px) - Only if not portal? "If the wall is a portal, draw the wall's line only (not the points)"
        if (!is_portal) {
            DrawRectangle(x1 - 2, y1 - 2, 5, 5, WHITE);
            DrawRectangle(x2 - 2, y2 - 2, 5, 5, WHITE);
        }
        
        // Draw Normal
        float dx = x2 - x1;
        float dy = y2 - y1;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 0) {
            float nx = dy / len;
            float ny = -dx / len;
            float mx = (x1 + x2) / 2.0f;
            float my = (y1 + y2) / 2.0f;
            DrawLine(mx, my, mx + nx * 5, my + ny * 5, WHITE);
        }
    }
    
    // 4. Draw Entities
    int max_slots = Entity_GetMaxSlots();
    for (int i = 0; i < max_slots; ++i) {
        Entity* e = Entity_GetBySlot(i);
        if (!e) continue;
        
        float ex = cx + (e->pos.x - view_pos.x) * zoom;
        float ey = cy - (e->pos.y - view_pos.y) * zoom;
        float half_size = 16.0f * zoom; // 32x32 bounding box
        
        Rectangle rect = { ex - half_size, ey - half_size, half_size * 2, half_size * 2 };
        
        DrawRectangleLinesEx(rect, 1.0f, RED);
        DrawLine(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height, RED); // Diagonal 1
        DrawLine(rect.x, rect.y + rect.height, rect.x + rect.width, rect.y, RED); // Diagonal 2
    }
    
    // 5. Draw Hovers
    if (hovered_entity_id != -1) {
        Entity* e = Entity_Get(hovered_entity_id);
        if (e) {
            float ex = cx + (e->pos.x - view_pos.x) * zoom;
            float ey = cy - (e->pos.y - view_pos.y) * zoom;
            float half_size = 16.0f * zoom;
            Rectangle rect = { ex - half_size, ey - half_size, half_size * 2, half_size * 2 };
            DrawRectangleLinesEx(rect, 2.0f, YELLOW);
            // Draw Diagoanls
            DrawLineEx((Vector2){rect.x, rect.y}, (Vector2){rect.x + rect.width, rect.y + rect.height}, 2.0f, YELLOW);
            DrawLineEx((Vector2){rect.x, rect.y + rect.height}, (Vector2){rect.x + rect.width, rect.y}, 2.0f, YELLOW);
        }
    } else if (hovered_point_index != -1 && hovered_wall_index != -1) {
        // If there is a hovered point and a hovered wall, still draw the wall's sector in orange,
        // but do not draw the hovered wall in yellow. Instead, draw only the hovered point in yellow.
        if (hovered_wall_index < map->wall_count) {
            // Draw sector of that wall in orange
            for(int s=0; s<map->sector_count; ++s) {
                Sector* sec = &map->sectors[s];
                if (hovered_wall_index >= sec->first_wall && hovered_wall_index < sec->first_wall + sec->num_walls) {
                    // Draw this sector in ORANGE.
                    for (u32 k=0; k<sec->num_walls; ++k) {
                        Wall* w = &map->walls[sec->first_wall + k];
                        Vec2 p1 = map->points[w->p1];
                        Vec2 p2 = map->points[w->p2];
                        float x1 = cx + (p1.x - view_pos.x) * zoom;
                        float y1 = cy - (p1.y - view_pos.y) * zoom;
                        float x2 = cx + (p2.x - view_pos.x) * zoom;
                        float y2 = cy - (p2.y - view_pos.y) * zoom;
                        DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 2.0f, ORANGE);
                    }
                    break; 
                }
            }
        }
        // Draw hovered point in yellow (will be drawn at end of hover section)
    } else if (hovered_wall_index != -1) {
        if (hovered_wall_index < map->wall_count) {
             // Draw sector of that wall in orange?
             // Need to find which sector this wall belongs to.
             // The map structure stores `first_wall` and `num_walls` in Sector, not `sector_id` in Wall.
             // Helper needed: GetSectorOfWall.
             // For now, simpler: we iterate all sectors and check if wall is in range.
             for(int s=0; s<map->sector_count; ++s) {
                 Sector* sec = &map->sectors[s];
                 if (hovered_wall_index >= sec->first_wall && hovered_wall_index < sec->first_wall + sec->num_walls) {
                     // Draw this sector in ORANGE.
                     // To draw a sector, we verify its walls.
                     for (u32 k=0; k<sec->num_walls; ++k) {
                         Wall* w = &map->walls[sec->first_wall + k];
                         Vec2 p1 = map->points[w->p1];
                         Vec2 p2 = map->points[w->p2];
                         float x1 = cx + (p1.x - view_pos.x) * zoom;
                         float y1 = cy - (p1.y - view_pos.y) * zoom;
                         float x2 = cx + (p2.x - view_pos.x) * zoom;
                         float y2 = cy - (p2.y - view_pos.y) * zoom;
                         DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 2.0f, ORANGE);
                     }
                     break; 
                 }
             }
        }
    } else if (hovered_sector != -1) {
        if (hovered_sector < map->sector_count) {
             Sector* sec = &map->sectors[hovered_sector];
             for (u32 k=0; k<sec->num_walls; ++k) {
                 Wall* w = &map->walls[sec->first_wall + k];
                 Vec2 p1 = map->points[w->p1];
                 Vec2 p2 = map->points[w->p2];
                 float x1 = cx + (p1.x - view_pos.x) * zoom;
                 float y1 = cy - (p1.y - view_pos.y) * zoom;
                 float x2 = cx + (p2.x - view_pos.x) * zoom;
                 float y2 = cy - (p2.y - view_pos.y) * zoom;
                 DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 2.0f, YELLOW);
             }
        }
    }
    
    // Draw hovered point in yellow (if there is one and no hovered entity)
    if (hovered_entity_id == -1 && hovered_point_index != -1) {
        if (hovered_point_index < map->point_count) {
            Vec2 pt = map->points[hovered_point_index];
            float px = cx + (pt.x - view_pos.x) * zoom;
            float py = cy - (pt.y - view_pos.y) * zoom;
            // Draw point as a square in yellow (11x11)
            DrawRectangle(px - 5, py - 5, 11, 11, YELLOW);
        }
    }
    
    // 6. Draw Selections
    if (selected_entity_id != -1) {
        Entity* e = Entity_Get(selected_entity_id);
        if (e) {
            float ex = cx + (e->pos.x - view_pos.x) * zoom;
            float ey = cy - (e->pos.y - view_pos.y) * zoom;
            float half_size = 16.0f * zoom;
            Rectangle rect = { ex - half_size, ey - half_size, half_size * 2, half_size * 2 };
            DrawRectangleLinesEx(rect, 2.0f, MAGENTA); // Bright Purple approx
            DrawLineEx((Vector2){rect.x, rect.y}, (Vector2){rect.x + rect.width, rect.y + rect.height}, 2.0f, MAGENTA);
            DrawLineEx((Vector2){rect.x, rect.y + rect.height}, (Vector2){rect.x + rect.width, rect.y}, 2.0f, MAGENTA);
        }
    }
    // Else if point selected
    else if (selected_point_index != -1) {
        // Draw selected point in bright cyan
        if (selected_point_index < map->point_count) {
            Vec2 pt = map->points[selected_point_index];
            float px = cx + (pt.x - view_pos.x) * zoom;
            float py = cy - (pt.y - view_pos.y) * zoom;
            // Draw point as a square in bright cyan (11x11)
            DrawRectangle(px - 5, py - 5, 11, 11, (Color){0, 255, 255, 255}); // Cyan
        }
    }
    // Else if wall selected (only draw if no selected point)
    else if (highlight_wall_index != -1) {
        // Draw sector dark green, wait, how do we know the sector?
        // Again, find sector.
         int sec_id = -1;
         for(int s=0; s<map->sector_count; ++s) {
             Sector* sec = &map->sectors[s];
             if (highlight_wall_index >= sec->first_wall && highlight_wall_index < sec->first_wall + sec->num_walls) {
                 sec_id = s;
                 break;
             }
         }
         
         if (sec_id != -1) {
             Sector* sec = &map->sectors[sec_id];
             Color lime_green = (Color){0, 255, 0, 255}; // Lime Green
             for (u32 k=0; k<sec->num_walls; ++k) {
                 Wall* w = &map->walls[sec->first_wall + k];
                 Vec2 p1 = map->points[w->p1];
                 Vec2 p2 = map->points[w->p2];
                 float x1 = cx + (p1.x - view_pos.x) * zoom;
                 float y1 = cy - (p1.y - view_pos.y) * zoom;
                 float x2 = cx + (p2.x - view_pos.x) * zoom;
                 float y2 = cy - (p2.y - view_pos.y) * zoom;
                 DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 2.0f, lime_green);
             }
         }
         
         // Draw Selected Wall in Bright Cyan
         if (highlight_wall_index < map->wall_count) {
             Wall* w = &map->walls[highlight_wall_index];
             Vec2 p1 = map->points[w->p1];
             Vec2 p2 = map->points[w->p2];
             float x1 = cx + (p1.x - view_pos.x) * zoom;
             float y1 = cy - (p1.y - view_pos.y) * zoom;
             float x2 = cx + (p2.x - view_pos.x) * zoom;
             float y2 = cy - (p2.y - view_pos.y) * zoom;
             DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 2.0f, (Color){0, 255, 255, 255}); // Cyan
         }
    }
    // Else if sector selected (highlight_sector is used for selection in current signature?)
    // Wait, the signature I have is: `highlight_sector`, `highlight_wall_index`...
    // The previous code mapped `highlight_sector` to `Editor_GetSelectedSectorID`.
    // So `highlight_sector` IS the selected sector.
    else if (highlight_sector != -1) {
         if (highlight_sector < map->sector_count) {
             Sector* sec = &map->sectors[highlight_sector];
             for (u32 k=0; k<sec->num_walls; ++k) {
                 Wall* w = &map->walls[sec->first_wall + k];
                 Vec2 p1 = map->points[w->p1];
                 Vec2 p2 = map->points[w->p2];
                 float x1 = cx + (p1.x - view_pos.x) * zoom;
                 float y1 = cy - (p1.y - view_pos.y) * zoom;
                 float x2 = cx + (p2.x - view_pos.x) * zoom;
                 float y2 = cy - (p2.y - view_pos.y) * zoom;
                 DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 2.0f, LIME);
             }
        }
    }
    
    // 7. If there is a hovered wall and NO hovered entity and NO hovered point, draw the hovered wall in yellow
    if (hovered_entity_id == -1 && hovered_point_index == -1 && hovered_wall_index != -1 && hovered_wall_index != highlight_wall_index) {
         if (hovered_wall_index < map->wall_count) {
             Wall* w = &map->walls[hovered_wall_index];
             Vec2 p1 = map->points[w->p1];
             Vec2 p2 = map->points[w->p2];
             float x1 = cx + (p1.x - view_pos.x) * zoom;
             float y1 = cy - (p1.y - view_pos.y) * zoom;
             float x2 = cx + (p2.x - view_pos.x) * zoom;
             float y2 = cy - (p2.y - view_pos.y) * zoom;
             DrawLineEx((Vector2){x1, y1}, (Vector2){x2, y2}, 2.0f, YELLOW);
         }
    }
    
    // 5. Draw Camera
    float scale = zoom; // Convert world units to pixels
    float L = 32.0f * scale;
    float half_W = 8.0f * scale; // Total width 16
    
    // Camera Screen Pos
    float cam_sx = cx + (cam.pos.x - view_pos.x) * zoom;
    float cam_sy = cy - (cam.pos.y - view_pos.y) * zoom;
    
    float ang = -cam.yaw;
    float c = cosf(ang);
    float s = sinf(ang);
    
    Vector2 fwd = { c, s };
    Vector2 side = { -s, c }; // Right vector (90 deg)
    
    // Tip at L distance forward from Camera Screen Pos
    Vector2 v_tip = { cam_sx + fwd.x * L, cam_sy + fwd.y * L };
    
    // Base corners
    Vector2 v_left = { cam_sx - side.x * half_W, cam_sy - side.y * half_W };
    Vector2 v_right = { cam_sx + side.x * half_W, cam_sy + side.y * half_W };
    
    Color col_cam = (Color){255, 0, 255, 255}; // Magenta
    DrawTriangle(v_tip, v_left, v_right, col_cam);
    
    EndScissorMode();
}