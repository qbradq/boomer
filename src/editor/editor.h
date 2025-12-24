#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#include <SDL.h>

struct Map;
struct Camera;

// Initialize the Editor system
void Editor_Init(SDL_Window* window, SDL_Renderer* renderer);

// Handle SDL Events for Editor
// Returns true if the editor consumed the event
bool Editor_HandleEvent(SDL_Event* event);

// Update Editor State (Input, Logic)
void Editor_Update(struct Map* map, struct Camera* cam);

// Render Editor UI
void Editor_Render(struct Map* map, struct Camera* cam);

// Shutdown Editor
void Editor_Shutdown(void);

// Check if Editor Mode is Active
bool Editor_IsActive(void);

// Toggle Editor Mode
void Editor_Toggle(void);

// Get Current View Mode (0 = 3D Game, 1 = 2D Map)
int Editor_GetViewMode(void);

int Editor_GetSelectedSectorID(void);

#endif
