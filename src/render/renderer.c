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
        
        Texture* wall_tex = Texture_Get(wall->texture_id);
        
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
            int wy_top = max(y_ceil, cy_top);
            int wy_bot = min(y_floor, cy_bot);
            
            if (wy_top < wy_bot) {
                if (portal) {
                    // Save for recursion
                    next_y_top[x] = wy_top;
                    next_y_bot[x] = wy_bot;
                } else {
                    // Draw Solid Wall
                    if (wall_tex) {
                        f32 iz = iz1 + (iz2 - iz1) * t_screen;
                        f32 uz = uz1 + (uz2 - uz1) * t_screen;
                        f32 u = uz / iz; 
                        
                        int tex_x = (int)u;
                        
                        f32 world_height = sector->ceil_height - sector->floor_height;
                        float v_scale = world_height * 64.0f;
                        float v_start = 0.0f;
                        float v_end = v_scale; 
                        float height = y_floor_f - y_ceil_f;
                        float v_step = (v_end - v_start) / height;
                        
                        Video_DrawTexturedColumn(x, wy_top, wy_bot - 1, wall_tex, tex_x, v_start + (wy_top - y_ceil_f) * v_step, v_step);
                    } else {
                        Video_DrawVertLine(x, wy_top, wy_bot - 1, (Color){100, 100, 100, 255});
                    }
                }
            } else {
                // Determine if we need to close the portal gap?
                // If wy_top >= wy_bot, the portal is effectively closed/occluded.
                // We should probably mark it as such for recursion.
                if (portal) {
                   next_y_top[x] = VIDEO_HEIGHT; // Invalid
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

