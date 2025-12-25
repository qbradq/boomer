#ifndef BOOMER_MAP_LOADER_H
#define BOOMER_MAP_LOADER_H

#include "world_types.h"
#include <stdbool.h>

// Load map from JSON file at path. 
// Populates the map structure. 
// Note: This operation may reset the texture system/cache to load map-specific textures.
bool Map_Load(const char* path, Map* out_map);

#endif // BOOMER_MAP_LOADER_H
