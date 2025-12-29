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
    char script_path[64];
    JSValue instance_js; // Reference to the Instance Object
} Entity;

typedef struct {
    u32 id;
    Vec3 pos;
    f32 yaw;
    char script_path[64];
} EntitySnapshot;

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

// Snapshot API
// Returns number of snapshots. allocs array in *out_snapshots. Caller must free.
int Entity_GetSnapshot(EntitySnapshot** out_snapshots);
void Entity_Restore(const EntitySnapshot* snapshots, int count);

#endif // BOOMER_ENTITY_H
