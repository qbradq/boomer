#ifndef BOOMER_ENTITY_H
#define BOOMER_ENTITY_H

#include "../core/types.h"

#include "../core/script_sys.h"

// Simple ECS-ish entity
typedef struct {
    u32 id;
    bool active;
    
    Vec3 pos;
    Vec3 vel;
    f32 yaw;
    
    // Scripting
    JSValue instance_js; // Reference to the Instance Object
} Entity;

void Entity_Init(void);
void Entity_Shutdown(void);
void Entity_Update(f32 dt);

// Spawns an entity using a script.
// The script should return a "Class" table (factory).
// Returns ID of new entity.
u32 Entity_Spawn(const char* script_path, Vec3 pos);

Entity* Entity_Get(u32 id);
Entity* Entity_GetBySlot(int slot);
int Entity_GetMaxSlots(void);

#endif // BOOMER_ENTITY_H
