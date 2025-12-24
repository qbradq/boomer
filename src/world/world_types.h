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
    
    // Future: Texture IDs, UV scales, etc.
} Wall;

typedef struct {
    f32 floor_height;
    f32 ceil_height;
    
    WallID first_wall;  // Index into Map.walls
    u32 num_walls;      // Number of walls in this sector
    
    // Future: Floor/Ceiling textures, light level
} Sector;

typedef struct {
    Wall*   walls;
    u32     wall_count;
    
    Sector* sectors;
    u32     sector_count;
} Map;

#endif // BOOMER_WORLD_TYPES_H
