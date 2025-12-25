#include "entity.h"
#include <stdio.h>
#include <string.h>

#define MAX_ENTITIES 1024

static Entity g_entities[MAX_ENTITIES];
static u32 g_next_id = 1; // 0 reserved

// --- JS Bindings ---

// Entity.SetPos(id, x, y, z)
static JSValue js_Entity_SetPos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 4) return JS_EXCEPTION;
    
    uint32_t id;
    if (JS_ToUint32(ctx, &id, argv[0])) return JS_EXCEPTION;
    
    double x, y, z;
    if (JS_ToFloat64(ctx, &x, argv[1])) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &y, argv[2])) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &z, argv[3])) return JS_EXCEPTION;
    
    Entity* e = Entity_Get(id);
    if (e) {
        e->pos = (Vec3){(f32)x, (f32)y, (f32)z};
    }
    return JS_UNDEFINED;
}

// {x,y,z} = Entity.GetPos(id)
static JSValue js_Entity_GetPos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_EXCEPTION;
    
    uint32_t id;
    if (JS_ToUint32(ctx, &id, argv[0])) return JS_EXCEPTION;
    
    Entity* e = Entity_Get(id);
    if (e) {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, e->pos.x));
        JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, e->pos.y));
        JS_SetPropertyStr(ctx, obj, "z", JS_NewFloat64(ctx, e->pos.z));
        return obj;
    }
    return JS_NULL;
}

void Entity_Init(void) {
    memset(g_entities, 0, sizeof(g_entities));
    g_next_id = 1;
    
    Script_RegisterFunc("Entity_SetPos", js_Entity_SetPos, 4);
    Script_RegisterFunc("Entity_GetPos", js_Entity_GetPos, 1);
    
    // In JS we can just use these global functions directly or wrap them in an object in a prelude script.
    // For now, let's expose them as global functions for the user scripts to wrap.
    // Or we can register a global object "Entity".
    
    // QuickJS RegisterFunc helper I wrote puts them on Global.
    // Let's create an Entity object on Global to match the old API properly.
    JSContext* ctx = Script_GetContext();
    if (!ctx) return;
    
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue entity_obj = JS_NewObject(ctx);
    
    JS_SetPropertyStr(ctx, entity_obj, "SetPos", JS_NewCFunction(ctx, js_Entity_SetPos, "SetPos", 4));
    JS_SetPropertyStr(ctx, entity_obj, "GetPos", JS_NewCFunction(ctx, js_Entity_GetPos, "GetPos", 1));
    
    JS_SetPropertyStr(ctx, global_obj, "Entity", entity_obj);
    JS_FreeValue(ctx, global_obj);
}

void Entity_Shutdown(void) {
    JSContext* ctx = Script_GetContext();
    if (!ctx) return;
    
    for (int i=0; i<MAX_ENTITIES; ++i) {
        if (g_entities[i].active) {
            JS_FreeValue(ctx, g_entities[i].instance_js);
            g_entities[i].active = false;
        }
    }
}

Entity* Entity_Get(u32 id) {
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
    
    JSContext* ctx = Script_GetContext();
    if (!ctx) return 0;
    
    // Load Script
    // Returns the value of the script execution.
    // We expect the script to either:
    // A) Return a Class Constructor (function)
    // B) Return a Factory Function
    // C) Return an Object (Prototype)
    // Let's assume it returns a Factory Function or Class.
    
    JSValue factory = Script_EvalFile(script_path);
    if (JS_IsException(factory)) {
        return 0; // Error printed by EvalFile
    }
    
    if (!JS_IsFunction(ctx, factory) && !JS_IsObject(factory)) {
        printf("Entity: Script '%s' did not return a function or object.\n", script_path);
        JS_FreeValue(ctx, factory);
        return 0;
    }
    
    // Create Instance
    // If it's a constructor, call 'new'.
    // If it's a factory function, call it.
    // QuickJS JS_CallConstructor vs JS_Call.
    
    JSValue instance;
    if (JS_IsConstructor(ctx, factory)) {
        instance = JS_CallConstructor(ctx, factory, 0, NULL);
    } else if (JS_IsFunction(ctx, factory)) {
         instance = JS_Call(ctx, factory, JS_UNDEFINED, 0, NULL);
    } else {
        // It's an object? Use it as prototype?
        // Or maybe it IS the instance (singleton)?
        // Let's assume generic object factory return for now.
        // Copying it might be complex.
        // Let's assume it's a factory function for now as standard.
        // If it's just an object, maybe we clone it (Object.create)?
        // For now, let's treat it as a prototype.
        // instance = Object.create(factory)
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue object_cls = JS_GetPropertyStr(ctx, global, "Object");
        JSValue create_fn = JS_GetPropertyStr(ctx, object_cls, "create");
        JSValue args[1] = { factory };
        instance = JS_Call(ctx, create_fn, object_cls, 1, args);
        
        JS_FreeValue(ctx, create_fn);
        JS_FreeValue(ctx, object_cls);
        JS_FreeValue(ctx, global);
    }
    
    JS_FreeValue(ctx, factory); // Done with factory
    
    if (JS_IsException(instance)) {
        printf("Entity: Failed to instantiate entity from '%s'\n", script_path);
        JSValue ex = JS_GetException(ctx);
        JS_FreeValue(ctx, ex); // Clear exception
        return 0;
    }
    
    // Setup Entity C struct
    Entity* e = &g_entities[slot];
    e->id = g_next_id++;
    e->active = true;
    e->pos = pos;
    e->vel = (Vec3){0,0,0};
    e->yaw = 0;
    
    // Set 'id' in Instance
    JS_SetPropertyStr(ctx, instance, "id", JS_NewInt32(ctx, e->id));
    
    // Store Reference
    e->instance_js = instance; // Takes ownership? Yes, we keep it.
    
    return e->id;
}

void Entity_Update(f32 dt) {
    JSContext* ctx = Script_GetContext();
    if (!ctx) return;
    
    for (int i=0; i<MAX_ENTITIES; ++i) {
        Entity* e = &g_entities[i];
        if (!e->active) continue;
        
        // Call instance.think(dt)
        JSValue think_func = JS_GetPropertyStr(ctx, e->instance_js, "think");
        if (JS_IsFunction(ctx, think_func)) {
            JSValue args[1];
            args[0] = JS_NewFloat64(ctx, dt);
            
            JSValue ret = JS_Call(ctx, think_func, e->instance_js, 1, args);
            
            if (JS_IsException(ret)) {
                printf("Entity %d Think Error\n", e->id);
                JSValue ex = JS_GetException(ctx);
                const char* s = JS_ToCString(ctx, ex);
                if (s) { printf("%s\n", s); JS_FreeCString(ctx, s); }
                JS_FreeValue(ctx, ex);
            }
            
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, args[0]);
        }
        JS_FreeValue(ctx, think_func);
        
        // Physics integration (simple)
        e->pos.x += e->vel.x * dt;
        e->pos.y += e->vel.y * dt;
        e->pos.z += e->vel.z * dt;
    }
}
