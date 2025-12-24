#ifndef BOOMER_WORLD_TYPES_H
#define BOOMER_WORLD_TYPES_H

#include "../core/types.h"

// Index into array, -1 if invalid/none
typedef i32 SectorID;
typedef i32 WallID;

typedef struct {
    Vec2 p1;            // Start vertex
    Vec2 p2;            // End vertex
    SectorID next_sector; // -1 if solid wall, otherwise ID of connected sector
    i32 texture_id;     // -1 if none
    
    // Future: Texture IDs, UV scales, etc.
} Wall;

typedef struct {
    f32 floor_height;
    f32 ceil_height;
    
    WallID first_wall;  // Index into Map.walls
    u32 num_walls;      // Number of walls in this sector
    
    i32 floor_tex_id; // -1 if none
    i32 ceil_tex_id;  // -1 if none
    
    // Future: Floor/Ceiling textures, light level
} Sector;

typedef struct {
    Wall*   walls;
    u32     wall_count;
    
    Sector* sectors;
    u32     sector_count;
} Map;

// Find which sector contains the point (x,y)
SectorID GetSectorAt(Map* map, Vec2 pos);

#endif // BOOMER_WORLD_TYPES_H
