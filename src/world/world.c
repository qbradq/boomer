#include "world_types.h"
#include "../core/math_utils.h"

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