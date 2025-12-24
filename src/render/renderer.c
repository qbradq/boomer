#include "renderer.h"
#include "../video/video.h"
#include "../core/math_utils.h"
#include <math.h>

#define FOV_H DEG2RAD(90.0f)
#define NEAR_Z 0.1f

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
// Input: Two points in Camera Space (p1, p2)
// Output: Two clipped points in Camera Space (out1, out2)
// Returns false if fully clipped
static bool ClipWall(Vec3 p1, Vec3 p2, Vec3* out1, Vec3* out2) {
    *out1 = p1;
    *out2 = p2;
    
    f32 n = NEAR_Z;
    
    // Both behind or on plane? (Reject)
    if (p1.x < n && p2.x < n) return false;
    
    // Both in front? (Accept)
    if (p1.x >= n && p2.x >= n) return true;
    
    // One behind, one in front (Clip)
    // Find intersection t where p(t).x = n
    // x(t) = x1 + (x2 - x1) * t = n
    // t = (n - x1) / (x2 - x1)
    
    f32 t = (n - p1.x) / (p2.x - p1.x);
    Vec3 intersect = vec3_lerp(p1, p2, t);
    intersect.x = n; // Force exact
    
    if (p1.x < n) {
        *out1 = intersect; // P1 was behind, replace it
    } else {
        *out2 = intersect; // P2 was behind, replace it
    }
    
    return true;
}

void Render_Frame(Camera cam, Map* map) {
    f32 scale = (VIDEO_WIDTH / 2.0f) / tanf(FOV_H / 2.0f);
    f32 center_x = VIDEO_WIDTH / 2.0f;
    f32 center_y = VIDEO_HEIGHT / 2.0f;

    for (u32 s = 0; s < map->sector_count; ++s) {
        Sector* sector = &map->sectors[s];
        Color wall_color = (s == 0) ? (Color){150, 150, 150, 255} : (Color){100, 100, 200, 255};
        
        for (u32 w = 0; w < sector->num_walls; ++w) {
            WallID wid = sector->first_wall + w;
            Wall* wall = &map->walls[wid];
            
            // 1. Transform both ends to Camera Space
            // Map data appears to be CCW (Right-to-Left on screen).
            // User mandates CW winding (Left-to-Right).
            // Swap P1/P2 to enforce CW winding so visible walls satisfy x1 < x2.
            Vec3 p1_world = { wall->p2.x, wall->p2.y, 0 }; 
            Vec3 p2_world = { wall->p1.x, wall->p1.y, 0 };
            
            Vec3 p1_cam = TransformToCamera(p1_world, cam);
            Vec3 p2_cam = TransformToCamera(p2_world, cam);
            
            // 2. Clip Wall
            Vec3 c1, c2;
            if (!ClipWall(p1_cam, p2_cam, &c1, &c2)) continue;
            
            // 3. Project X coordinates (Z is still needed for height)
            f32 x1 = center_x + (c1.y / c1.x) * scale;
            f32 x2 = center_x + (c2.y / c2.x) * scale;
            
            // 4. Back-face Culling (Screen Space)
            // Visible walls wind Clockwise (Left to Right).
            // So x1 < x2 is Visible.
            if (x1 >= x2) continue;
            
            // 5. Calculate Heights
            f32 ceil_h = sector->ceil_height - cam.pos.z;
            f32 floor_h = sector->floor_height - cam.pos.z;
            
            f32 y1a = center_y - (ceil_h / c1.x) * scale;
            f32 y1b = center_y - (floor_h / c1.x) * scale;
            f32 y2a = center_y - (ceil_h / c2.x) * scale;
            f32 y2b = center_y - (floor_h / c2.x) * scale;
            
            // 6. Rasterize
            // We know x1 < x2 here.
            int ix1 = (int)ceilf(x1);
            int ix2 = (int)ceilf(x2);
            
            if (ix1 < 0) ix1 = 0;
            if (ix2 >= VIDEO_WIDTH) ix2 = VIDEO_WIDTH;
            
            for (int x = ix1; x < ix2; ++x) {
                // T from 0 (at x1) to 1 (at x2)
                f32 t = (x - x1) / (x2 - x1);
                
                f32 y_ceil = y1a + (y2a - y1a) * t;
                f32 y_floor = y1b + (y2b - y1b) * t;
                
                Video_DrawVertLine(x, (int)y_ceil, (int)y_floor, wall_color);
            }
            
            // Wireframe
            Video_DrawVertLine(ix1, (int)y1a, (int)y1b, (Color){255, 255, 255, 255});
            Video_DrawVertLine(ix2-1, (int)y2a, (int)y2b, (Color){255, 255, 255, 255});
        }
    }
}
