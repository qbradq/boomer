#ifndef WORLD_H
#define WORLD_H

#include "world_types.h"
#include "../core/math_utils.h"

// Returns the sector ID at the given position, or -1 if none
SectorID GetSectorAt(Map* map, Vec2 pos);

// Deep copy src to dest. dest should be empty or freed.
void Map_Clone(Map* dest, const Map* src);

// Free all dynamic memory in map and zero counts.
void Map_Free(Map* map);

// Replace dest content with a deep copy of src. Frees old dest content.
void Map_Restore(Map* dest, const Map* src);

#endif
