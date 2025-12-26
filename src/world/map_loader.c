#include "map_loader.h"
#include "../core/script_sys.h" // For JS_ParseJSON
#include "../core/fs.h"
#include "../video/texture.h"
#include "../game/entity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double GetNumber(JSContext* ctx, JSValue obj, const char* name, double def) {
    JSValue val = JS_GetPropertyStr(ctx, obj, name);
    double ret = def;
    if (JS_IsNumber(val)) {
        JS_ToFloat64(ctx, &ret, val);
    }
    JS_FreeValue(ctx, val);
    return ret;
}

static int GetInt(JSContext* ctx, JSValue obj, const char* name, int def) {
    return (int)GetNumber(ctx, obj, name, (double)def);
}

static Vec2 GetVec2(JSContext* ctx, JSValue arr) {
    Vec2 v = {0,0};
    if (JS_IsArray(arr)) {
        JSValue v0 = JS_GetPropertyUint32(ctx, arr, 0);
        JSValue v1 = JS_GetPropertyUint32(ctx, arr, 1);
        double d0 = 0, d1 = 0;
        JS_ToFloat64(ctx, &d0, v0);
        JS_ToFloat64(ctx, &d1, v1);
        v.x = (f32)d0;
        v.y = (f32)d1;
        JS_FreeValue(ctx, v0);
        JS_FreeValue(ctx, v1);
    }
    return v;
}

static Vec3 GetVec3(JSContext* ctx, JSValue arr) {
    Vec3 v = {0,0,0};
    if (JS_IsArray(arr)) {
        JSValue v0 = JS_GetPropertyUint32(ctx, arr, 0);
        JSValue v1 = JS_GetPropertyUint32(ctx, arr, 1);
        JSValue v2 = JS_GetPropertyUint32(ctx, arr, 2);
        double d0=0, d1=0, d2=0;
        JS_ToFloat64(ctx, &d0, v0);
        JS_ToFloat64(ctx, &d1, v1);
        JS_ToFloat64(ctx, &d2, v2);
        v.x = (f32)d0;
        v.y = (f32)d1;
        v.z = (f32)d2;
        JS_FreeValue(ctx, v0);
        JS_FreeValue(ctx, v1);
        JS_FreeValue(ctx, v2);
    }
    return v;
}

bool Map_Load(const char* path, Map* out_map) {
    JSContext* ctx = Script_GetContext();
    if (!ctx) {
        printf("Map_Load: Script system not initialized.\n");
        return false;
    }

    char full_map_path[256];
    snprintf(full_map_path, sizeof(full_map_path), "maps/%s", path);

    size_t size;
    char* data = FS_ReadFile(full_map_path, &size);
    if (!data) {
        printf("Map_Load: Could not read file '%s'\n", full_map_path);
        return false;
    }

    JSValue val = JS_ParseJSON(ctx, data, size, full_map_path);
    FS_FreeFile(data);

    if (JS_IsException(val)) {
        printf("Map_Load: JSON Parse Error in '%s'\n", path);
        JSValue ex = JS_GetException(ctx);
        JS_FreeValue(ctx, ex);
        return false;
    }
    
    // 0. Load Points
    JSValue points = JS_GetPropertyStr(ctx, val, "points");
    if (JS_IsArray(points)) {
        JSValue len_val = JS_GetPropertyStr(ctx, points, "length");
        int point_count = 0;
        JS_ToInt32(ctx, &point_count, len_val);
        JS_FreeValue(ctx, len_val);
        
        if (point_count > 0) {
            out_map->point_count = point_count;
            out_map->points = (Vec2*)malloc(sizeof(Vec2) * point_count);
            for(int i = 0; i < point_count; ++i) {
                JSValue p_val = JS_GetPropertyUint32(ctx, points, i);
                out_map->points[i] = GetVec2(ctx, p_val);
                JS_FreeValue(ctx, p_val);
            }
        }
    } else {
        // Fallback for old maps or if missing: 
        // We'll allocate points dynamically if needed, but for now strict.
        printf("Map_Load: 'points' array missing in JSON.\n");
        out_map->point_count = 0;
        out_map->points = NULL;
    }
    JS_FreeValue(ctx, points);

    
    // 1. Load Textures
    JSValue textures = JS_GetPropertyStr(ctx, val, "textures");
    int tex_count = 0;
    TextureID* global_tex_ids = NULL;
    
    if (JS_IsArray(textures)) {
        JSValue len = JS_GetPropertyStr(ctx, textures, "length");
        int length = 0;
        JS_ToInt32(ctx, &length, len);
        JS_FreeValue(ctx, len);
        
        if (length > 0) {
            global_tex_ids = (TextureID*)malloc(sizeof(TextureID) * length);
            tex_count = length;
            
            for (int i = 0; i < length; ++i) {
                JSValue item = JS_GetPropertyUint32(ctx, textures, i);
                JSValue path_val = JS_GetPropertyStr(ctx, item, "path");
                const char* tex_path_str = JS_ToCString(ctx, path_val);
                
                if (tex_path_str) {
                    char full_tex_path[256];
                    snprintf(full_tex_path, sizeof(full_tex_path), "textures/%s", tex_path_str);
                    global_tex_ids[i] = Texture_Load(full_tex_path);
                    JS_FreeCString(ctx, tex_path_str);
                } else {
                    global_tex_ids[i] = -1;
                }
                
                JS_FreeValue(ctx, path_val);
                JS_FreeValue(ctx, item);
            }
        }
    }
    JS_FreeValue(ctx, textures);

    // 2. Load Geometry
    JSValue sectors = JS_GetPropertyStr(ctx, val, "sectors");
    if (JS_IsArray(sectors)) {
        JSValue len_val = JS_GetPropertyStr(ctx, sectors, "length");
        int sector_count = 0;
        JS_ToInt32(ctx, &sector_count, len_val);
        JS_FreeValue(ctx, len_val);
        
        if (sector_count > 0) {
            out_map->sector_count = sector_count;
            out_map->sectors = (Sector*)malloc(sizeof(Sector) * sector_count);
            
            // First pass: Count walls to allocate
            int total_walls = 0;
            for (int i = 0; i < sector_count; ++i) {
                JSValue idx_s = JS_GetPropertyUint32(ctx, sectors, i);
                JSValue walls_arr = JS_GetPropertyStr(ctx, idx_s, "walls");
                
                if (JS_IsArray(walls_arr)) {
                    JSValue w_len_val = JS_GetPropertyStr(ctx, walls_arr, "length");
                    int w_len = 0;
                    JS_ToInt32(ctx, &w_len, w_len_val);
                    total_walls += w_len;
                    JS_FreeValue(ctx, w_len_val);
                }
                
                JS_FreeValue(ctx, walls_arr);
                JS_FreeValue(ctx, idx_s);
            }
            
            out_map->wall_count = total_walls;
            out_map->walls = (Wall*)malloc(sizeof(Wall) * total_walls);
            
            // Second pass: Populate
            int current_wall_idx = 0;
            for (int i = 0; i < sector_count; ++i) {
                JSValue s_obj = JS_GetPropertyUint32(ctx, sectors, i);
                Sector* sec = &out_map->sectors[i];
                
                sec->floor_height = (f32)GetNumber(ctx, s_obj, "floor_height", 0.0);
                sec->ceil_height = (f32)GetNumber(ctx, s_obj, "ceil_height", 3.0);
                
                int f_tid = GetInt(ctx, s_obj, "floor_tex", -1);
                int c_tid = GetInt(ctx, s_obj, "ceil_tex", -1);
                
                sec->floor_tex_id = (f_tid >= 0 && f_tid < tex_count) ? global_tex_ids[f_tid] : -1;
                sec->ceil_tex_id = (c_tid >= 0 && c_tid < tex_count) ? global_tex_ids[c_tid] : -1;
                
                sec->first_wall = current_wall_idx;
                
                JSValue w_arr = JS_GetPropertyStr(ctx, s_obj, "walls");
                JSValue w_len_val = JS_GetPropertyStr(ctx, w_arr, "length");
                int w_len = 0;
                JS_ToInt32(ctx, &w_len, w_len_val);
                JS_FreeValue(ctx, w_len_val);
                
                sec->num_walls = w_len;
                
                for (int w = 0; w < w_len; ++w) {
                    JSValue w_obj = JS_GetPropertyUint32(ctx, w_arr, w);
                    Wall* wall = &out_map->walls[current_wall_idx + w];
                    
                    wall->p1 = GetInt(ctx, w_obj, "p1", -1);
                    wall->p2 = GetInt(ctx, w_obj, "p2", -1);
                    
                    wall->next_sector = GetInt(ctx, w_obj, "portal", -1);
                    
                    int tid = GetInt(ctx, w_obj, "tex", -1);
                    wall->texture_id = (tid >= 0 && tid < tex_count) ? global_tex_ids[tid] : -1;
                    
                    wall->top_texture_id = wall->texture_id;
                    wall->bottom_texture_id = wall->texture_id;
                    
                    JS_FreeValue(ctx, w_obj);
                }
                current_wall_idx += w_len;
                
                JS_FreeValue(ctx, w_arr);
                JS_FreeValue(ctx, s_obj);
            }
        }
    }
    JS_FreeValue(ctx, sectors);
    
    // 3. Load Entities
    JSValue entities = JS_GetPropertyStr(ctx, val, "entities");
    if (JS_IsArray(entities)) {
        JSValue len_val = JS_GetPropertyStr(ctx, entities, "length");
        int ent_count = 0;
        JS_ToInt32(ctx, &ent_count, len_val);
        JS_FreeValue(ctx, len_val);
        
        for (int i = 0; i < ent_count; ++i) {
            JSValue e_obj = JS_GetPropertyUint32(ctx, entities, i);
            
            JSValue script_val = JS_GetPropertyStr(ctx, e_obj, "script");
            const char* script = JS_ToCString(ctx, script_val);
            
            JSValue pos_val = JS_GetPropertyStr(ctx, e_obj, "pos");
            Vec3 pos = GetVec3(ctx, pos_val);
            
            if (script) {
                Entity_Spawn(script, pos);
                JS_FreeCString(ctx, script);
            }
            
            JS_FreeValue(ctx, pos_val);
            JS_FreeValue(ctx, script_val);
            JS_FreeValue(ctx, e_obj);
        }
    }
    JS_FreeValue(ctx, entities);

    if (global_tex_ids) free(global_tex_ids);
    JS_FreeValue(ctx, val);
    
    printf("Map_Load: Loaded '%s' (%d points, %d sectors, %d walls)\n", path, out_map->point_count, out_map->sector_count, out_map->wall_count);
    return true;
}