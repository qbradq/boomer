#include "renderer.h"
#include "../video/video.h"
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

static bool ClipWall(Vec3 p1, Vec3 p2, Vec3* out1, Vec3* out2) {
    *out1 = p1;
    *out2 = p2;
    f32 n = NEAR_Z;
    if (p1.x < n && p2.x < n) return false;
    if (p1.x >= n && p2.x >= n) return true;
    f32 t = (n - p1.x) / (p2.x - p1.x);
    Vec3 intersect = vec3_lerp(p1, p2, t);
    intersect.x = n;
    if (p1.x < n) *out1 = intersect;
    else *out2 = intersect;
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
    
    // For portals, we need to sort walls? 
    // Usually standard engines just iterate. If we draw front-to-back (recursively), Z-buffer isn't strictly needed for columns, 
    // but we need to track "drawn" columns or just clip properly.
    // 1D Clipping: We pass [min_x, max_x] window. We only draw in that window.
    // Neighbors restrict this window further.

    for (u32 w = 0; w < sector->num_walls; ++w) {
        WallID wid = sector->first_wall + w;
        Wall* wall = &map->walls[wid];
        
        // 1. Transform & Clip
        // Enforce CW winding by using p2->p1 (based on previous findings)
        Vec3 p1_world = { wall->p2.x, wall->p2.y, 0 }; 
        Vec3 p2_world = { wall->p1.x, wall->p1.y, 0 };
        
        Vec3 p1_cam = TransformToCamera(p1_world, cam);
        Vec3 p2_cam = TransformToCamera(p2_world, cam);
        
        Vec3 c1, c2;
        if (!ClipWall(p1_cam, p2_cam, &c1, &c2)) continue;
        
        // 2. Project X
        f32 x1 = center_x + (c1.y / c1.x) * scale;
        f32 x2 = center_x + (c2.y / c2.x) * scale;
        
        // 3. Cull
        if (x1 >= x2) continue; // Back-facing
        
        // 4. Clip to Window
        int ix1 = (int)ceilf(x1);
        int ix2 = (int)ceilf(x2);
        
        int draw_x1 = (ix1 < min_x) ? min_x : ix1;
        int draw_x2 = (ix2 > max_x) ? max_x : ix2;
        
        if (draw_x1 >= draw_x2) continue; // Outside window
        
        // 5. Calculate Heights
        f32 ceil_h = sector->ceil_height - cam.pos.z;
        f32 floor_h = sector->floor_height - cam.pos.z;
        
        f32 y1a = center_y - (ceil_h / c1.x) * scale;
        f32 y1b = center_y - (floor_h / c1.x) * scale;
        f32 y2a = center_y - (ceil_h / c2.x) * scale;
        f32 y2b = center_y - (floor_h / c2.x) * scale;
        
        // 6. Draw
        if (wall->next_sector == -1) {
            // -- Solid Wall --
            for (int x = draw_x1; x < draw_x2; ++x) {
                f32 t = (x - x1) / (x2 - x1);
                f32 y_ceil = y1a + (y2a - y1a) * t;
                f32 y_floor = y1b + (y2b - y1b) * t;
                
                // Draw Solid Column
                Video_DrawVertLine(x, (int)y_ceil, (int)y_floor, (Color){100, 100, 100, 255});
            }
        } else {
            // -- Portal --
            // Recurse!
            // Calculate Next Sector Heights if we want to draw step up/down (later)
            // For now, assume open portal.
            
            // We might need to draw Floor/Ceiling steps if adjacent sector height is different.
            // Skipping for Step 1 of Phase 4.
            
            // Recurse into neighbor with clipped window
            RenderSector(map, cam, wall->next_sector, draw_x1, draw_x2, depth + 1);
            
            // Draw Wireframe on Portal edges just to see it
            // Video_DrawVertLine(draw_x1, ..., (Color){255,0,0,255});
        }
    }
}

void Render_Frame(Camera cam, Map* map) {
    SectorID start_sector = GetSectorAt(map, (Vec2){cam.pos.x, cam.pos.y});
    if (start_sector == -1) start_sector = 0; // Fallback
    
    Video_Clear((Color){20, 20, 30, 255});
    RenderSector(map, cam, start_sector, 0, VIDEO_WIDTH, 0);
}

