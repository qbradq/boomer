#ifndef BOOMER_SCRIPT_SYS_H
#define BOOMER_SCRIPT_SYS_H

#include "types.h"
#include "quickjs.h"

// Initialize Scripting System (QuickJS)
bool Script_Init(void);

// Shutdown Scripting System
void Script_Shutdown(void);

// Load and Execute a script file from the FS
// Returns the result of the script (JSValue).
// Caller must free the returned value using JS_FreeValue.
// Returns JS_EXCEPTION on failure.
JSValue Script_EvalFile(const char* path);

// Get the global JS Context
JSContext* Script_GetContext(void);

// Register a C function as a global function
void Script_RegisterFunc(const char* name, JSCFunction* func, int length);

#endif // BOOMER_SCRIPT_SYS_H
