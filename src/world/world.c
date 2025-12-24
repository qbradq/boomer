#include "world_types.h"
#include "../core/math_utils.h"

// Point in Polygon test (Ray casting algorithm)
static bool IsPointInSector(Sector* sector, Map* map, Vec2 p) {
    bool inside = false;
    for (u32 i = 0; i < sector->num_walls; ++i) {
        Wall* w = &map->walls[sector->first_wall + i];
        
        // Check intersection with horizontal ray from p to +infinity
        // Edge: w->p1 to w->p2
        if (((w->p1.y > p.y) != (w->p2.y > p.y)) &&
            (p.x < (w->p2.x - w->p1.x) * (p.y - w->p1.y) / (w->p2.y - w->p1.y) + w->p1.x)) {
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
