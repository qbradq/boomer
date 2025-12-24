#include "renderer.h"
#include "../video/video.h"
#include "../video/texture.h"
#include "../core/math_utils.h"
#include <math.h>

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

// Recursive Sector Render
static void RenderSector(Map* map, Camera cam, SectorID sector_id, int min_x, int max_x, int depth) {
    if (depth > MAX_RECURSION) return;
    if (min_x >= max_x) return;

    Sector* sector = &map->sectors[sector_id];
    f32 scale = (VIDEO_WIDTH / 2.0f) / tanf(FOV_H / 2.0f);
    f32 center_x = VIDEO_WIDTH / 2.0f;
    f32 center_y = VIDEO_HEIGHT / 2.0f;
    
    for (u32 w = 0; w < sector->num_walls; ++w) {
        WallID wid = sector->first_wall + w;
        Wall* wall = &map->walls[wid];
        
        // 1. Transform & Clip
        // Enforce CW winding
        Vec3 p1_world = { wall->p2.x, wall->p2.y, 0 }; 
        Vec3 p2_world = { wall->p1.x, wall->p1.y, 0 };
        
        // Calculate World Length for Texture Scaling (1 unit = 64 pixels)
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
        
        // 3. Cull
        if (x1 >= x2) continue;
        
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
        
        // Texture Setup
        Texture* tex = Texture_Get(wall->texture_id);
        
        // Perspective Correct Interpolation Setup
        // We interpolate 1/z (iz) and u/z (uiz)
        // At c1: z = c1.x
        // At c2: z = c2.x
        
        f32 iz1 = 1.0f / c1.x;
        f32 iz2 = 1.0f / c2.x;
        
        // U Coordinates
        // Scale: 1 unit = 64 pixels
        f32 u_scale = wall_len * 64.0f;
        f32 u1 = t1_clip * u_scale;
        f32 u2 = t2_clip * u_scale;
        
        // u/z
        f32 uz1 = u1 * iz1;
        f32 uz2 = u2 * iz2;
        
        // 6. Draw
        if (wall->next_sector == -1) {
            // -- Solid Wall --
            for (int x = draw_x1; x < draw_x2; ++x) {
                // Screen T
                f32 t_screen = (x - x1) / (x2 - x1);
                
                f32 y_ceil = y1a + (y2a - y1a) * t_screen;
                f32 y_floor = y1b + (y2b - y1b) * t_screen;
                
                if (tex) {
                    // Perspective Correct U
                    f32 iz = iz1 + (iz2 - iz1) * t_screen;
                    f32 uz = uz1 + (uz2 - uz1) * t_screen;
                    f32 u = uz / iz; // Recenter U
                    
                    int tex_x = (int)u;
                    
                    // V Mapping (Linear per column)
                    // Scale: 1 unit = 64 pixels
                    f32 world_height = sector->ceil_height - sector->floor_height;
                    float v_scale = world_height * 64.0f;
                    
                    float v_start = 0.0f;
                    float v_end = v_scale; 
                    
                    // The column goes from y_ceil (screen) to y_floor (screen)
                    // We need v_step per screen pixel.
                    float height = y_floor - y_ceil;
                    float v_step = (v_end - v_start) / height;
                    
                    Video_DrawTexturedColumn(x, (int)y_ceil, (int)y_floor, tex, tex_x, v_start, v_step);
                    
                } else {
                    // Solid Color Fallback
                    Video_DrawVertLine(x, (int)y_ceil, (int)y_floor, (Color){100, 100, 100, 255});
                }
            }
        } else {
            // -- Portal --
            RenderSector(map, cam, wall->next_sector, draw_x1, draw_x2, depth + 1);
        }
    }
}

void Render_Frame(Camera cam, Map* map) {
    SectorID start_sector = GetSectorAt(map, (Vec2){cam.pos.x, cam.pos.y});
    if (start_sector == -1) start_sector = 0; // Fallback
    
    Video_Clear((Color){20, 20, 30, 255});
    RenderSector(map, cam, start_sector, 0, VIDEO_WIDTH, 0);
}

