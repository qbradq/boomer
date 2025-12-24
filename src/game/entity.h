#ifndef BOOMER_ENTITY_H
#define BOOMER_ENTITY_H

#include "../core/types.h"

// Simple ECS-ish entity
typedef struct {
    u32 id;
    bool active;
    
    Vec3 pos;
    Vec3 vel;
    f32 yaw;
    
    // Scripting
    int lua_ref; // Reference to the Instance Table in Lua Registry
} Entity;

void Entity_Init(void);
void Entity_Update(f32 dt);

// Spawns an entity using a script.
// The script should return a "Class" table (factory).
// Returns ID of new entity.
u32 Entity_Spawn(const char* script_path, Vec3 pos);

Entity* Entity_Get(u32 id);

#endif // BOOMER_ENTITY_H
