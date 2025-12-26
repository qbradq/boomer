#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>

struct Map;
#include <raylib.h> // For Rectangle

struct Map;
struct GameCamera;

// Initialize the Editor system
void Editor_Init(void);

// Get the viewport for the game/map rendering (excluding editor UI)
Rectangle GetGameViewRect(void);

void Editor_Init(void);

// Editor Input Frame Control
void Editor_InputBegin(void);
void Editor_InputEnd(void);

// Handle Input for Editor 
// Returns true if the editor consumed the input
bool Editor_HandleInput(void);

// Update Editor State (Input, Logic)
void Editor_Update(struct Map* map, struct GameCamera* cam);

// Render Editor UI
void Editor_Render(struct Map* map, struct GameCamera* cam);

// Shutdown Editor
void Editor_Shutdown(void);

// Check if Editor Mode is Active
bool Editor_IsActive(void);

// Toggle Editor Mode
void Editor_Toggle(void);

// Get Current View Mode (0 = 3D Game, 1 = 2D Map)
int Editor_GetViewMode(void);

int Editor_GetSelectedSectorID(void);
int Editor_GetSelectedWallIndex(void);
int Editor_GetHoveredSectorID(void);
int Editor_GetHoveredWallIndex(void);

#endif
