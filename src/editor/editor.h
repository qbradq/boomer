#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#include <SDL.h>

// Initialize the Editor system
void Editor_Init(SDL_Window* window, SDL_Renderer* renderer);

// Handle SDL Events for Editor
// Returns true if the editor consumed the event
bool Editor_HandleEvent(SDL_Event* event);

// Update Editor State
void Editor_Update(void);

// Render Editor UI
void Editor_Render(void);

// Shutdown Editor
void Editor_Shutdown(void);

// Check if Editor Mode is Active
bool Editor_IsActive(void);

// Toggle Editor Mode
void Editor_Toggle(void);

// Get Current View Mode (0 = 3D Game, 1 = 2D Map)
int Editor_GetViewMode(void);

#endif
