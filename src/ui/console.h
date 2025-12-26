#ifndef BOOMER_CONSOLE_H
#define BOOMER_CONSOLE_H

#include "../core/types.h"
#include <stdbool.h>

// Initialize console
bool Console_Init(void);

// Shutdown console
void Console_Shutdown(void);

// Log message
void Console_Log(const char* fmt, ...);

// Draw console overlay
void Console_Draw(void);

// Handle input (Toggle toggles visibility)
// Returns true if console consumed the event
bool Console_HandleEvent(void); // Params removed mostly as Raylib polls input

// Update console state (animations)
void Console_Update(float dt);

bool Console_IsActive(void);

void Console_SetMapLoaded(bool loaded);

#endif // BOOMER_CONSOLE_H
