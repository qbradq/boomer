#ifndef WORLD_H
#define WORLD_H

#include "world_types.h"
#include "../core/math_utils.h"

// Returns the sector ID at the given position, or -1 if none
SectorID GetSectorAt(Map* map, Vec2 pos);

#endif
