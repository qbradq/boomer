#ifndef BOOMER_LUA_SYS_H
#define BOOMER_LUA_SYS_H

#include "types.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// Initialize Lua State
bool Lua_Init(void);

// Shutdown Lua State
void Lua_Shutdown(void);

// Load and Execute a script file from the FS
// Returns true on success.
// If ret_results > 0, leaves that many return values on the stack.
// Otherwise clears stack.
bool Lua_DoFile(const char* path);

// Get the global Lua state
lua_State* Lua_GetState(void);

// Register a C function in the global usage
void Lua_RegisterFunc(const char* name, lua_CFunction func);

#endif // BOOMER_LUA_SYS_H
