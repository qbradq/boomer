#include "editor.h"
#include "raylib.h"
#include "raygui.h"
#include <stdio.h>

#define TOOLBAR_HEIGHT 40
#define STATUSBAR_HEIGHT 24
#define SIDEBAR_WIDTH 320

static bool is_active = false;

// Editor State
typedef enum {
    TOOL_SELECT,
    TOOL_SECTOR,
    TOOL_ENTITY
} EditorTool;

typedef enum {
    SEL_NONE,
    SEL_SECTOR,
    SEL_WALL,
    SEL_ENTITY
} SelectionType;

static EditorTool current_tool = TOOL_SELECT;
static float zoom_level = 1.0f;
static int grid_size = 32;
static bool view_3d = true;

static SelectionType sel_type = SEL_NONE;
static int sel_id = -1;

// Layout
Rectangle GetGameViewRect(void) {
    if (!is_active) {
        return (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
    }
    
    return (Rectangle){
        0, 
        TOOLBAR_HEIGHT, 
        (float)GetScreenWidth() - SIDEBAR_WIDTH, 
        (float)GetScreenHeight() - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT
    };
}

void Editor_Init(void) {
    GuiSetStyle(DEFAULT, TEXT_SIZE, 12);
}

void Editor_InputBegin(void) {
}

void Editor_InputEnd(void) {
}

bool Editor_HandleInput(void) {
    if (!is_active) return false;
    
    // If mouse is over UI, return true
    Vector2 mouse = GetMousePosition();
    if (mouse.y < TOOLBAR_HEIGHT) return true;
    if (mouse.y > GetScreenHeight() - STATUSBAR_HEIGHT) return true;
    if (mouse.x > GetScreenWidth() - SIDEBAR_WIDTH) return true;
    
    return false;
}

void Editor_Update(struct Map* map, struct GameCamera* cam) {
    (void)map;
    (void)cam;
}

static void DrawToolbar(void) {
    GuiPanel((Rectangle){0, 0, (float)GetScreenWidth(), TOOLBAR_HEIGHT}, NULL);
    
    float x = 5;
    float y = 5;
    float w = 30;
    float h = 30;
    float pad = 5;
    
    // File
    if (GuiButton((Rectangle){x, y, w, h}, "#001#")) printf("Stub: New\n"); x += w + pad;
    if (GuiButton((Rectangle){x, y, w, h}, "#002#")) printf("Stub: Open\n"); x += w + pad;
    if (GuiButton((Rectangle){x, y, w, h}, "#003#")) printf("Stub: Save\n"); x += w + pad;
    
    x += pad * 2; // Separator
    
    // Edit
    if (GuiButton((Rectangle){x, y, w, h}, "#004#")) printf("Stub: Undo\n"); x += w + pad;
    if (GuiButton((Rectangle){x, y, w, h}, "#005#")) printf("Stub: Redo\n"); x += w + pad;
    if (GuiButton((Rectangle){x, y, w, h}, "#016#")) printf("Stub: Copy\n"); x += w + pad;
    if (GuiButton((Rectangle){x, y, w, h}, "#017#")) printf("Stub: Cut\n"); x += w + pad;
    if (GuiButton((Rectangle){x, y, w, h}, "#018#")) printf("Stub: Paste\n"); x += w + pad;
    
    x += pad * 2; // Separator
    
    // View
    if (GuiButton((Rectangle){x, y, w, h}, view_3d ? "3D" : "2D")) {
        view_3d = !view_3d;
    } 
    x += w + pad;
    
    if (GuiButton((Rectangle){x, y, w, h}, "#020#")) {
        zoom_level *= 1.2f;
    }
    x += w + pad;
    
    if (GuiButton((Rectangle){x, y, w, h}, "#021#")) {
        zoom_level /= 1.2f;
    }
    x += w + pad;
    
    if (GuiButton((Rectangle){x, y, w, h}, "G-")) {
        grid_size /= 2;
        if (grid_size < 1) grid_size = 1;
    }
    x += w + pad;
    
    if (GuiButton((Rectangle){x, y, w, h}, "G+")) {
        grid_size *= 2;
    }
    x += w + pad;
    
    x += pad * 2; // Separator
    
    // Tools
    bool is_sel = (current_tool == TOOL_SELECT);
    if (GuiToggle((Rectangle){x, y, w, h}, "#022#", &is_sel)) current_tool = TOOL_SELECT; x += w + pad;
    
    bool is_sec = (current_tool == TOOL_SECTOR);
    if (GuiToggle((Rectangle){x, y, w, h}, "#023#", &is_sec)) current_tool = TOOL_SECTOR; x += w + pad;
    
    bool is_ent = (current_tool == TOOL_ENTITY);
    if (GuiToggle((Rectangle){x, y, w, h}, "#024#", &is_ent)) current_tool = TOOL_ENTITY; x += w + pad;
    
    x += pad * 2;
    
    if (GuiButton((Rectangle){x, y, w, h}, "#112#")) {
        Editor_Toggle();
    }
}

static void DrawSidebar(void) {
    Rectangle bounds = { 
        (float)GetScreenWidth() - SIDEBAR_WIDTH, 
        TOOLBAR_HEIGHT, 
        SIDEBAR_WIDTH, 
        (float)GetScreenHeight() - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT 
    };
    
    GuiPanel(bounds, "Properties");
    
    float x = bounds.x + 10;
    float y = bounds.y + 30;
    float w = bounds.width - 20;
    
    if (sel_type == SEL_NONE) {
        GuiLabel((Rectangle){x, y, w, 20}, "No selection");
        return;
    }

    if (sel_type == SEL_SECTOR) {
        GuiGroupBox((Rectangle){x, y, w, 150}, "Sector Properties");
        // Stubs for height, textures, etc.
    } else if (sel_type == SEL_WALL) {
        GuiGroupBox((Rectangle){x, y, w, 150}, "Wall Properties");
    } else if (sel_type == SEL_ENTITY) {
        GuiGroupBox((Rectangle){x, y, w, 150}, "Entity Properties");
    }
}

static void DrawStatusBar(void) {
    Rectangle bounds = {
        0,
        (float)GetScreenHeight() - STATUSBAR_HEIGHT,
        (float)GetScreenWidth(),
        STATUSBAR_HEIGHT
    };
    
    GuiPanel(bounds, NULL);
    
    // Fixed width elements from right to left
    float cur_x = bounds.x + bounds.width;
    float element_w = 100;
    
    // Tool
    cur_x -= element_w;
    const char* tool_name = "SELECT";
    if (current_tool == TOOL_SECTOR) tool_name = "SECTOR";
    if (current_tool == TOOL_ENTITY) tool_name = "ENTITY";
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, TextFormat("TOOL: %s", tool_name));
    
    // Pos
    cur_x -= element_w;
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, "POS: (0,0)");
    
    // Zoom
    cur_x -= element_w;
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, TextFormat("ZOOM: %.2f", zoom_level));
    
    // Grid
    cur_x -= element_w;
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, TextFormat("GRID: %d", grid_size));
    
    // File (Last element, remaining width)
    float file_w = cur_x - bounds.x - 10;
    GuiLabel((Rectangle){bounds.x + 10, bounds.y, file_w, bounds.height}, "FILE: maps/test.json");
}

void Editor_Render(struct Map* map, struct GameCamera* cam) {
    if (!is_active) return;
    
    DrawToolbar();
    DrawSidebar();
    DrawStatusBar();
}

void Editor_Shutdown(void) {
    printf("Editor Shutdown.\n");
}

bool Editor_IsActive(void) {
    return is_active;
}

void Editor_Toggle(void) {
    is_active = !is_active;
}

int Editor_GetViewMode(void) {
    return view_3d ? 0 : 1;
}

int Editor_GetSelectedSectorID(void) {
    return (sel_type == SEL_SECTOR) ? sel_id : -1;
}

int Editor_GetSelectedWallIndex(void) {
    return (sel_type == SEL_WALL) ? sel_id : -1;
}

int Editor_GetHoveredSectorID(void) {
    return -1;
}

int Editor_GetHoveredWallIndex(void) {
    return -1;
}
