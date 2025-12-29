#include "world_types.h"
#include "world.h"
#include "../core/math_utils.h"
#include <stdlib.h>
#include <string.h>

// Point in Polygon test (Ray casting algorithm)
static bool IsPointInSector(Sector* sector, Map* map, Vec2 p) {
    bool inside = false;
    for (u32 i = 0; i < sector->num_walls; ++i) {
        Wall* w = &map->walls[sector->first_wall + i];
        Vec2 p1 = map->points[w->p1];
        Vec2 p2 = map->points[w->p2];
        
        // Check intersection with horizontal ray from p to +infinity
        // Edge: p1 to p2
        if (((p1.y > p.y) != (p2.y > p.y)) &&
            (p.x < (p2.x - p1.x) * (p.y - p1.y) / (p2.y - p1.y) + p1.x)) {
            inside = !inside;
        }
    }
    return inside;
}

SectorID GetSectorAt(Map* map, Vec2 pos) {
    for (i32 i = 0; i < (i32)map->sector_count; ++i) {
        if (IsPointInSector(&map->sectors[i], map, pos)) {
            return i;
        }
    }
    return -1;
}

void Map_Clone(Map* dest, const Map* src) {
    // Copy counts
    dest->point_count = src->point_count;
    dest->wall_count = src->wall_count;
    dest->sector_count = src->sector_count;

    // Allocate and Copy Points
    if (src->point_count > 0 && src->points) {
        dest->points = (Vec2*)malloc(sizeof(Vec2) * src->point_count);
        memcpy(dest->points, src->points, sizeof(Vec2) * src->point_count);
    } else {
        dest->points = NULL;
    }

    // Allocate and Copy Walls
    if (src->wall_count > 0 && src->walls) {
        dest->walls = (Wall*)malloc(sizeof(Wall) * src->wall_count);
        memcpy(dest->walls, src->walls, sizeof(Wall) * src->wall_count);
    } else {
        dest->walls = NULL;
    }

    // Allocate and Copy Sectors
    if (src->sector_count > 0 && src->sectors) {
        dest->sectors = (Sector*)malloc(sizeof(Sector) * src->sector_count);
        memcpy(dest->sectors, src->sectors, sizeof(Sector) * src->sector_count);
    } else {
        dest->sectors = NULL;
    }
}

void Map_Free(Map* map) {
    if (map->points) free(map->points);
    if (map->walls) free(map->walls);
    if (map->sectors) free(map->sectors);

    map->points = NULL;
    map->walls = NULL;
    map->sectors = NULL;
    map->point_count = 0;
    map->wall_count = 0;
    map->sector_count = 0;
}

void Map_Restore(Map* dest, const Map* src) {
    Map_Free(dest);
    Map_Clone(dest, src);
}