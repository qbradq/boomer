#include "entity.h"
#include "../core/lua_sys.h"
#include <stdio.h>
#include <string.h>

#define MAX_ENTITIES 1024

static Entity g_entities[MAX_ENTITIES];
static u32 g_next_id = 1; // 0 reserved

// --- C Bindings for Lua ---

// Entity.SetPos(id, x, y, z)
static int l_Entity_SetPos(lua_State* L) {
    u32 id = (u32)luaL_checkinteger(L, 1);
    f32 x = (f32)luaL_checknumber(L, 2);
    f32 y = (f32)luaL_checknumber(L, 3);
    f32 z = (f32)luaL_checknumber(L, 4);
    
    Entity* e = Entity_Get(id);
    if (e) {
        e->pos = (Vec3){x, y, z};
    }
    return 0;
}

// x,y,z = Entity.GetPos(id)
static int l_Entity_GetPos(lua_State* L) {
    u32 id = (u32)luaL_checkinteger(L, 1);
    Entity* e = Entity_Get(id);
    if (e) {
        lua_pushnumber(L, e->pos.x);
        lua_pushnumber(L, e->pos.y);
        lua_pushnumber(L, e->pos.z);
        return 3;
    }
    return 0;
}

void Entity_Init(void) {
    memset(g_entities, 0, sizeof(g_entities));
    g_next_id = 1;
    
    // Register Global "Entity" table/api
    lua_State* L = Lua_GetState();
    if (!L) return;
    
    lua_newtable(L); // Entity table
    
    lua_pushcfunction(L, l_Entity_SetPos);
    lua_setfield(L, -2, "SetPos");
    
    lua_pushcfunction(L, l_Entity_GetPos);
    lua_setfield(L, -2, "GetPos");
    
    lua_setglobal(L, "Entity"); // Set as global 'Entity'
}

Entity* Entity_Get(u32 id) {
    // Simple direct lookup if we assume slots? No, ID isn't index.
    // For now simple O(N) lookup/slot find or just direct index if ID maps to index (with generation check).
    // Let's just linearly search, simpler for 1 file.
    if (id == 0) return NULL;
    for (int i=0; i<MAX_ENTITIES; ++i) {
        if (g_entities[i].id == id && g_entities[i].active) return &g_entities[i];
    }
    return NULL;
}

u32 Entity_Spawn(const char* script_path, Vec3 pos) {
    // Find free slot
    int slot = -1;
    for (int i=0; i<MAX_ENTITIES; ++i) {
        if (!g_entities[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) {
        printf("Entity: Max entities reached!\n");
        return 0;
    }
    
    lua_State* L = Lua_GetState();
    if (!L) return 0;
    
    // Load Script
    // Expected to return a Class Table
    if (!Lua_DoFile(script_path)) {
        return 0;
    }
    
    // The script result (Table) is now at top of stack (if DoFile left it, which my DoFile returns MULTRET)
    // Wait, Lua_DoFile in lua_sys.c calls pcall with 0, MULTRET.
    // So if script returns 'return { ... }', it is on stack.
    
    if (!lua_istable(L, -1)) {
        printf("Entity: Script '%s' did not return a table.\n", script_path);
        lua_pop(L, 1);
        return 0;
    }
    
    // Create Instance
    // 1. New Table (Instance)
    lua_newtable(L); 
    
    // 2. Set Metatable (Class) -> Instance with __index = Class
    lua_pushvalue(L, -2); // Copy Class table
    lua_setfield(L, -2, "__index"); // { __index = Class } (Wait, usually metatable IS the table with __index)
    // Actually, usually:
    // Class.__index = Class
    // setmetatable(Instance, Class)
    
    // Let's assume the script returns a table 'Class' where Class.__index = Class is set by the script?
    // Or we force it here.
    lua_pushvalue(L, -2); // Check Class
    lua_setfield(L, -3, "__index"); // Class.__index = Class (Modified the Class table!)
    
    lua_pushvalue(L, -2); // Class
    lua_setmetatable(L, -2); // setmetatable(Instance, Class)
    
    // 3. Setup Entity C struct
    Entity* e = &g_entities[slot];
    e->id = g_next_id++;
    e->active = true;
    e->pos = pos;
    e->vel = (Vec3){0,0,0};
    e->yaw = 0;
    
    // 4. Set 'id' in Instance
    lua_pushinteger(L, e->id);
    lua_setfield(L, -2, "id");
    
    // 5. Store Reference
    // Stack: [Class] [Instance]
    e->lua_ref = luaL_ref(L, LUA_REGISTRYINDEX); // Pops Instance
    lua_pop(L, 1); // Pop Class
    
    return e->id;
}

void Entity_Update(f32 dt) {
    lua_State* L = Lua_GetState();
    if (!L) return;
    
    for (int i=0; i<MAX_ENTITIES; ++i) {
        Entity* e = &g_entities[i];
        if (!e->active) continue;
        
        // Call instance:think(dt)
        
        // 1. Get Instance
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->lua_ref); // Push Instance
        
        // 2. Get 'think' function
        lua_getfield(L, -1, "think"); // Push think
        
        if (lua_isfunction(L, -1)) {
            // 3. Push 'self' (Instance)
            lua_pushvalue(L, -2); 
            
            // 4. Push dt
            lua_pushnumber(L, dt);
            
            // Call: think(self, dt)
            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                printf("Entity %d Think Error: %s\n", e->id, lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1); // Pop nil/non-func
        }
        
        lua_pop(L, 1); // Pop Instance
        
        // Physics integration (simple)
        e->pos.x += e->vel.x * dt;
        e->pos.y += e->vel.y * dt;
        e->pos.z += e->vel.z * dt;
    }
}
