#ifndef BOOMER_RENDERER_H
#define BOOMER_RENDERER_H

#include "../core/types.h"
#include "../world/world_types.h"

typedef struct Camera {
    Vec3 pos;    // x, y, z
    f32  yaw;    // horizontal angle in radians
    // f32 pitch; // vertical angle (later)
} Camera;

// Initialize renderer resources
void Renderer_Init(void);

// Project world space vertex to screen space
// Returns true if the vertex is behind the camera (and should be clipped/ignored)
bool WorldToScreen(Vec3 world_pos, Camera cam, Vec2* screen_out);

// Render the map from the camera's perspective// Render the full scene
void Render_Frame(Camera cam, Map* map);

// Render Top-Down Map (High Resolution via SDL)
struct SDL_Renderer;
void Render_Map2D(struct SDL_Renderer* ren, Map* map, Camera cam, int x, int y, int w, int h, float zoom, int highlight_sector);

// Helper
SectorID GetSectorAt(Map* map, Vec2 pos);

#endif // BOOMER_RENDERER_H
