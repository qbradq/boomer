#include "lua_sys.h"
#include "fs.h"
#include <stdio.h>
#include <stdlib.h>

static lua_State* L = NULL;

bool Lua_Init(void) {
    if (L) Lua_Shutdown();
    
    L = luaL_newstate();
    if (!L) {
        printf("Lua: Failed to init state.\n");
        return false;
    }
    
    luaL_openlibs(L);
    printf("Lua: Initialized %s\n", LUA_VERSION);
    return true;
}

void Lua_Shutdown(void) {
    if (L) {
        lua_close(L);
        L = NULL;
    }
}

lua_State* Lua_GetState(void) {
    return L;
}

bool Lua_DoFile(const char* path) {
    if (!L) return false;
    
    size_t size;
    char* data = FS_ReadFile(path, &size);
    if (!data) {
        printf("Lua: Could not read script '%s'\n", path);
        return false;
    }
    
    // Load buffer (compiles script)
    // Name starts with @ to tell Lua it's a filename
    char chunk_name[256];
    snprintf(chunk_name, sizeof(chunk_name), "@%s", path);
    
    if (luaL_loadbuffer(L, data, size, chunk_name) != LUA_OK) {
        printf("Lua: Parse Error in '%s': %s\n", path, lua_tostring(L, -1));
        lua_pop(L, 1); // pop error
        FS_FreeFile(data);
        return false;
    }
    
    FS_FreeFile(data);
    
    // Run script (Protected Call)
    // 0 arguments, LUA_MULTRET results, 0 error handler
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        printf("Lua: Execution Error in '%s': %s\n", path, lua_tostring(L, -1));
        lua_pop(L, 1); // pop error
        return false;
    }
    
    return true;
}

void Lua_RegisterFunc(const char* name, lua_CFunction func) {
    if (!L) return;
    lua_register(L, name, func);
}
