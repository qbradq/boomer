#ifndef BOOMER_RENDERER_H
#define BOOMER_RENDERER_H

#include "../core/types.h"
#include "../world/world_types.h"

typedef struct GameCamera {
    Vec3 pos;    // x, y, z
    f32  yaw;    // horizontal angle in radians
    // f32 pitch; // vertical angle (later)
} GameCamera;

// Initialize renderer resources
void Renderer_Init(void);

// Project world space vertex to screen space
// Returns true if the vertex is behind the camera (and should be clipped/ignored)
bool WorldToScreen(Vec3 world_pos, GameCamera cam, Vec2* screen_out);

// Render the map from the camera's perspective// Render the full scene
void Render_Frame(GameCamera cam, Map* map);

// Render Top-Down Map (High Resolution via Raylib)
void Render_Map2D(Map* map, GameCamera cam, Vec2 view_pos, int x, int y, int w, int h, float zoom, int grid_size, int highlight_sector, int highlight_wall_index, int hovered_sector, int hovered_wall_index, int selected_entity_id, int hovered_entity_id, int hovered_point_index, int selected_point_index, bool is_move_invalid);

// Helper
SectorID GetSectorAt(Map* map, Vec2 pos);

#endif // BOOMER_RENDERER_H
