#include "editor.h"
#include <stdio.h>

static bool is_active = false;

void Editor_Init(void) {
    printf("Editor Initialized (Stubbed).\n");
}

void Editor_InputBegin(void) {
}

void Editor_InputEnd(void) {
}

bool Editor_HandleInput(void) {
    return false;
}

void Editor_Update(struct Map* map, struct GameCamera* cam) {
    (void)map;
    (void)cam;
}

void Editor_Render(struct Map* map, struct GameCamera* cam) {
    if (!is_active) return;
    (void)map;
    (void)cam;
    // Stub: Draw a simple text saying Editor is disabled?
    // Using Raylib (once included) or just nothing for now.
}

void Editor_Shutdown(void) {
    printf("Editor Shutdown (Stubbed).\n");
}

bool Editor_IsActive(void) {
    return is_active;
}

void Editor_Toggle(void) {
    is_active = !is_active;
    printf("Editor Mode: %s (Stubbed)\n", is_active ? "ON" : "OFF");
}

int Editor_GetViewMode(void) {
    return 0; // Always 3D view
}

int Editor_GetSelectedSectorID(void) {
    return -1;
}

int Editor_GetSelectedWallIndex(void) {
    return -1;
}

int Editor_GetHoveredSectorID(void) {
    return -1;
}

int Editor_GetHoveredWallIndex(void) {
    return -1;
}
