#include "editor.h"
#include "raylib.h"
#include "raygui.h"
#include <stdio.h>
#include <math.h>
#include "../world/world_types.h"
#include "../game/entity.h"
#include <stdlib.h>
#include <string.h>
#include "../render/renderer.h"

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
    SEL_ENTITY,
    SEL_POINT
} SelectionType;

static EditorTool current_tool = TOOL_SELECT;
static float zoom_level = 1.0f;
static int grid_size = 32;
static bool view_3d = true;

static SelectionType sel_type = SEL_NONE;
static int sel_id = -1;
static int hovered_sector = -1;
static int hovered_wall = -1;
static int hovered_entity = -1;
static int hovered_point = -1;

// Drag State
typedef struct {
    Vec2 mouse_start;
    
    // Stored geometry state
    Vec2* original_points; // For Sector (all points), Wall (2 points), Point (1 point)
    int num_stored_points;
    
    // Stored entity state
    Vec3 original_entity_pos;
} DragState;

static bool is_dragging = false;
static bool drag_valid = true;
static DragState drag_state = {0};

static void DragState_Free(void) {
    if (drag_state.original_points) {
        free(drag_state.original_points);
        drag_state.original_points = NULL;
    }
    drag_state.num_stored_points = 0;
}

// View State
static Vec2 view_pos = {0};
static bool view_initialized = false;

Vec2 Editor_GetViewPos(void) {
    return view_pos;
}

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


// Helper for distance point->line segment
static float DistToLine(Vector2 p, Vector2 l1, Vector2 l2) {
    float l2_sq = (l2.x - l1.x)*(l2.x - l1.x) + (l2.y - l1.y)*(l2.y - l1.y);
    if (l2_sq == 0) return sqrtf((p.x - l1.x)*(p.x - l1.x) + (p.y - l1.y)*(p.y - l1.y));
    
    // t = [(p - l1) . (l2 - l1)] / |l2 - l1|^2
    float t = ((p.x - l1.x)*(l2.x - l1.x) + (p.y - l1.y)*(l2.y - l1.y)) / l2_sq;
    
    if (t < 0) t = 0;
    else if (t > 1) t = 1;
    
    Vector2 proj = { l1.x + t * (l2.x - l1.x), l1.y + t * (l2.y - l1.y) };
    return sqrtf((p.x - proj.x)*(p.x - proj.x) + (p.y - proj.y)*(p.y - proj.y));
}

static float SnapToGrid(float val, int grid) {
    return roundf(val / (float)grid) * (float)grid;
}

static Vec2 SnapVecToGrid(Vec2 v, int grid) {
    return (Vec2){ SnapToGrid(v.x, grid), SnapToGrid(v.y, grid) };
}

// Check point in sector logic (simplified raycast or using GetSectorAt if reliable)
// Using GetSectorAt from world logic is best, but let's re-verify it works for a given Point.
// Assuming GetSectorAt works in world space.

// --- Drag Logic ---

static void Editor_StartDrag(struct Map* map, Vector2 mouse_world) {
    if (sel_type == SEL_NONE || sel_id == -1) return;
    
    is_dragging = true;
    drag_valid = true;
    drag_state.mouse_start = (Vec2){mouse_world.x, mouse_world.y};
    drag_state.original_points = NULL;
    drag_state.num_stored_points = 0;
    
    if (sel_type == SEL_ENTITY) {
        Entity* e = Entity_Get(sel_id);
        if (e) {
            drag_state.original_entity_pos = e->pos;
        } else {
            is_dragging = false;
        }
    } else if (sel_type == SEL_POINT) {
        if (sel_id < (int)map->point_count) {
            drag_state.num_stored_points = 1;
            drag_state.original_points = malloc(sizeof(Vec2));
            drag_state.original_points[0] = map->points[sel_id];
        } else {
            is_dragging = false;
        }
    } else if (sel_type == SEL_WALL) {
        if (sel_id < (int)map->wall_count) {
            drag_state.num_stored_points = 2;
            drag_state.original_points = malloc(sizeof(Vec2) * 2);
            Wall* w = &map->walls[sel_id];
            drag_state.original_points[0] = map->points[w->p1];
            drag_state.original_points[1] = map->points[w->p2];
        } else {
            is_dragging = false;
        }
    } else if (sel_type == SEL_SECTOR) {
        if (sel_id < (int)map->sector_count) {
            Sector* s = &map->sectors[sel_id];
            drag_state.num_stored_points = s->num_walls;
            drag_state.original_points = malloc(sizeof(Vec2) * s->num_walls);
            for (u32 i = 0; i < s->num_walls; ++i) {
                Wall* w = &map->walls[s->first_wall + i];
                drag_state.original_points[i] = map->points[w->p1];
            }
        } else {
            is_dragging = false;
        }
    }
}

static void Editor_UpdateDrag(struct Map* map, Vector2 mouse_world) {
    if (!is_dragging) return;
    
    Vector2 delta = { mouse_world.x - drag_state.mouse_start.x, mouse_world.y - drag_state.mouse_start.y };
    drag_valid = true; // Assume valid until proven otherwise
    
    if (sel_type == SEL_ENTITY) {
        Entity* e = Entity_Get(sel_id);
        if (e) {
            // Snap to grid
            Vec2 dest_2d = (Vec2){ drag_state.original_entity_pos.x + delta.x, drag_state.original_entity_pos.y + delta.y };
            dest_2d = SnapVecToGrid(dest_2d, grid_size);
            
            e->pos.x = dest_2d.x;
            e->pos.y = dest_2d.y;
            
            // Validate: Must be in a sector
            if (GetSectorAt(map, dest_2d) == -1) {
                drag_valid = false;
            }
        }
    } else if (sel_type == SEL_POINT) {
        // Point: Snap point to grid
        Vec2 dest = (Vec2){ drag_state.original_points[0].x + delta.x, drag_state.original_points[0].y + delta.y };
        dest = SnapVecToGrid(dest, grid_size);
        
        map->points[sel_id] = dest;
    } else if (sel_type == SEL_WALL) {
        // Wall: Snap start point, maintain direction
        Wall* w = &map->walls[sel_id];
        Vec2 p1_start = drag_state.original_points[0];
        Vec2 p2_start = drag_state.original_points[1];
        
        Vec2 p1_target = (Vec2){ p1_start.x + delta.x, p1_start.y + delta.y };
        Vec2 p1_snapped = SnapVecToGrid(p1_target, grid_size);
        
        Vec2 move_delta = (Vec2){ p1_snapped.x - p1_start.x, p1_snapped.y - p1_start.y };
        
        map->points[w->p1] = p1_snapped;
        map->points[w->p2] = (Vec2){ p2_start.x + move_delta.x, p2_start.y + move_delta.y };
        
    } else if (sel_type == SEL_SECTOR) {
        // Sector: Snap first point, relative move others
        Sector* s = &map->sectors[sel_id];
        Vec2 p0_start = drag_state.original_points[0];
        
        Vec2 p0_target = (Vec2){ p0_start.x + delta.x, p0_start.y + delta.y };
        Vec2 p0_snapped = SnapVecToGrid(p0_target, grid_size);
        
        Vec2 move_delta = (Vec2){ p0_snapped.x - p0_start.x, p0_snapped.y - p0_start.y };
        
        for (u32 i = 0; i < s->num_walls; ++i) {
            Wall* w = &map->walls[s->first_wall + i];
            map->points[w->p1] = (Vec2){ drag_state.original_points[i].x + move_delta.x, drag_state.original_points[i].y + move_delta.y };
        }
    }
}

static void Editor_CancelDrag(struct Map* map) {
    if (!is_dragging) return;
    
    // Restore
    if (sel_type == SEL_ENTITY) {
        Entity* e = Entity_Get(sel_id);
        if (e) e->pos = drag_state.original_entity_pos;
    } else if (sel_type == SEL_POINT) {
        map->points[sel_id] = drag_state.original_points[0];
    } else if (sel_type == SEL_WALL) {
        Wall* w = &map->walls[sel_id];
        map->points[w->p1] = drag_state.original_points[0];
        map->points[w->p2] = drag_state.original_points[1];
    } else if (sel_type == SEL_SECTOR) {
        Sector* s = &map->sectors[sel_id];
        for (u32 i = 0; i < s->num_walls; ++i) {
            Wall* w = &map->walls[s->first_wall + i];
            map->points[w->p1] = drag_state.original_points[i];
        }
    }
    
    is_dragging = false;
    DragState_Free();
}

static void Editor_EndDrag(struct Map* map) {
    if (!is_dragging) return;
    
    if (!drag_valid) {
        Editor_CancelDrag(map);
        return;
    }
    
    // Apply is already done implicitly by UpdateDrag modifying the map directly.
    // Just cleanup.
    is_dragging = false;
    DragState_Free();
}

void Editor_Update(struct Map* map, struct GameCamera* cam) {
    // 0. Update Limits
    if (zoom_level < 1.0f/32.0f) zoom_level = 1.0f/32.0f;
    if (zoom_level > 32.0f) zoom_level = 32.0f;
    
    if (grid_size < 1) grid_size = 1;
    if (grid_size > 1024) grid_size = 1024;

    if (!is_active) return;
    
    // World Mouse Pos
    Rectangle game_rect = GetGameViewRect();
    // Sync view on first frame
    if (!view_initialized) {
        view_pos.x = cam->pos.x;
        view_pos.y = cam->pos.y;
        view_initialized = true;
    }

    Vector2 mouse_s = GetMousePosition();

    // Zoom control with Mouse Wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        if (wheel > 0) zoom_level *= 2.0f;
        else zoom_level /= 2.0f;
    }

    // Panning with Right Mouse Button or Drag Cancel
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        if (is_dragging) {
             Editor_CancelDrag(map);
        } else {
            Vector2 delta = GetMouseDelta();
            view_pos.x -= delta.x / zoom_level;
            view_pos.y += delta.y / zoom_level;
        }
    }
    
    if (IsKeyPressed(KEY_ESCAPE) && is_dragging) {
        Editor_CancelDrag(map);
    }

    // Teleport with Middle Mouse Button
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
         Vector2 mouse_pos = GetMousePosition();
         if (CheckCollisionPointRec(mouse_pos, game_rect)) {
              // Convert to World using view_pos
              float wx = view_pos.x + (mouse_pos.x - (game_rect.x + game_rect.width/2)) / zoom_level;
              float wy = view_pos.y - (mouse_pos.y - (game_rect.y + game_rect.height/2)) / zoom_level;
              cam->pos.x = wx;
              cam->pos.y = wy;
              
              // Center view on teleport
              view_pos.x = wx;
              view_pos.y = wy;
              
              int s = GetSectorAt(map, (Vec2){wx, wy});
              if (s != -1) {
                  cam->pos.z = map->sectors[s].floor_height + 50.0f;
              }
         }
    }
    
    // If outside view, ignore, UNLESS dragging
    if (!CheckCollisionPointRec(mouse_s, game_rect) && !is_dragging) {
        hovered_entity = -1;
        hovered_wall = -1;
        hovered_sector = -1;
        hovered_point = -1;
        return;
    }
    
    // Calculate World Pos:
    // ScreenX = cx + (WorldX - CamX) * Zoom
    // WorldX = (ScreenX - cx) / Zoom + CamX
    float cx = game_rect.x + game_rect.width / 2.0f;
    float cy = game_rect.y + game_rect.height / 2.0f;
    
    float wx = (mouse_s.x - cx) / zoom_level + view_pos.x;
    float wy = view_pos.y - (mouse_s.y - cy) / zoom_level;
    
    // Drag Update
    if (is_dragging) {
        Editor_UpdateDrag(map, (Vector2){wx, wy});
        
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            Editor_EndDrag(map);
        }
        return; // Skip hover logic while dragging
    }
    
    // Hover Logic
    hovered_entity = -1;
    hovered_wall = -1;
    hovered_sector = -1;
    hovered_point = -1;
    
    // 1. Entities (32x32 box)
    int max_slots = Entity_GetMaxSlots();
    for (int i = 0; i < max_slots; ++i) {
        Entity* e = Entity_GetBySlot(i);
        if (!e) continue;
        
        // Check AABB centered at e->pos
        // Half size 16
        if (wx >= e->pos.x - 16 && wx <= e->pos.x + 16 &&
            wy >= e->pos.y - 16 && wy <= e->pos.y + 16) {
            hovered_entity = e->id;
            break; // First one on top?
        }
    }
    
    // 2. Points (10px screen tolerance)
    float screen_tol = 10.0f;
    
    if (hovered_entity == -1) {
        // Check all points for hover
        for (int i = 0; i < (int)map->point_count; ++i) {
            Vec2 pt = map->points[i];
            
            // Project to Screen (use view_pos for consistency with rendering)
            float sx = cx + (pt.x - view_pos.x) * zoom_level;
            float sy = cy - (pt.y - view_pos.y) * zoom_level;
            
            // Calculate distance from mouse to point
            float dx = mouse_s.x - sx;
            float dy = mouse_s.y - sy;
            float dist = sqrtf(dx * dx + dy * dy);
            
            if (dist <= screen_tol) {
                hovered_point = i;
                break; // First point that's close enough
            }
        }
    }
    
    // 3. Walls (10px screen tolerance)
    if (hovered_entity == -1) {
        for (int i = 0; i < (int)map->wall_count; ++i) {
            Wall* w = &map->walls[i];
            Vec2 p1 = map->points[w->p1];
            Vec2 p2 = map->points[w->p2];
            
            // Project to Screen (use view_pos for consistency with rendering)
            float sx1 = cx + (p1.x - view_pos.x) * zoom_level;
            float sy1 = cy - (p1.y - view_pos.y) * zoom_level;
            float sx2 = cx + (p2.x - view_pos.x) * zoom_level;
            float sy2 = cy - (p2.y - view_pos.y) * zoom_level;
            
            float screen_dist = DistToLine(mouse_s, (Vector2){sx1, sy1}, (Vector2){sx2, sy2});
            
            if (screen_dist <= screen_tol) {
                // Special handling for Portals
                if (w->next_sector != -1) {
                    // Must be contained within the wall's sector.
                    int owner_sec = -1;
                    // Find owner sector
                    for (int s=0; s<(int)map->sector_count; ++s) {
                        if (i >= map->sectors[s].first_wall && i < (int)(map->sectors[s].first_wall + map->sectors[s].num_walls)) {
                            owner_sec = s;
                            break;
                        }
                    }
                    if (owner_sec != -1) {
                        if (GetSectorAt(map, (Vec2){wx, wy}) == owner_sec) {
                             hovered_wall = i;
                             break;
                        }
                    }
                } else {
                    hovered_wall = i;
                    break;
                }
            }
        }
    }
    
    // 4. Sectors
    if (hovered_entity == -1 && hovered_wall == -1 && hovered_point == -1) {
        // Point in sector
        int sec = GetSectorAt(map, (Vec2){wx, wy});
        if (sec != -1) {
            hovered_sector = sec;
        }
    }
    
    // Click Handling
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (hovered_entity != -1) {
            sel_type = SEL_ENTITY;
            sel_id = hovered_entity;
        } else if (hovered_point != -1) {
            sel_type = SEL_POINT;
            sel_id = hovered_point;
        } else if (hovered_wall != -1) {
            sel_type = SEL_WALL;
            sel_id = hovered_wall;
        } else if (hovered_sector != -1) {
            sel_type = SEL_SECTOR;
            sel_id = hovered_sector;
        } else {
            sel_type = SEL_NONE;
            sel_id = -1;
        }
        
        // Attempt Start Drag
        if (sel_type != SEL_NONE && current_tool == TOOL_SELECT) {
             Editor_StartDrag(map, (Vector2){wx, wy});
        }
    }
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
    } else if (sel_type == SEL_POINT) {
        GuiGroupBox((Rectangle){x, y, w, 150}, "Point Properties");
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
    
    // 1. Tool (Rightmost)
    cur_x -= element_w;
    const char* tool_name = "SELECT";
    if (current_tool == TOOL_SECTOR) tool_name = "SECTOR";
    if (current_tool == TOOL_ENTITY) tool_name = "ENTITY";
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, TextFormat("TOOL: %s", tool_name));
    
    // 2. Pos
    cur_x -= element_w;
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, "POS: (0,0)");
    
    // 3. Zoom
    cur_x -= element_w;
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, TextFormat("ZOOM: %.2f", zoom_level));
    
    // 4. Grid
    cur_x -= element_w;
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, TextFormat("GRID: %d", grid_size));
    
    // 5. Selection Info (To the left of Grid)
    cur_x -= element_w;
    
    const char* status_text = "No Selection";
    static char buf[64];
    
    if (sel_type == SEL_ENTITY) {
        snprintf(buf, 64, "Entity %d", sel_id);
        status_text = buf;
    } else if (sel_type == SEL_WALL) {
        snprintf(buf, 64, "Wall %d", sel_id);
        status_text = buf;
    } else if (sel_type == SEL_SECTOR) {
        snprintf(buf, 64, "Sector %d", sel_id);
        status_text = buf;
    } else if (sel_type == SEL_POINT) {
        snprintf(buf, 64, "Point %d", sel_id);
        status_text = buf;
    }
    
    GuiLabel((Rectangle){cur_x, bounds.y, element_w, bounds.height}, status_text);
    
    // Left-aligned Element: Map Name / Info
    // "replaced the filename display on the left" -> restore it.
    // We don't have map name in Editor struct yet, just hardcode "Map: Untitled" or similar.
    // Or if we want to be fancy, finding map name would require refactoring.
    GuiLabel((Rectangle){bounds.x + 10, bounds.y, 200, bounds.height}, "Map: demo.map");
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

int Editor_GetHoveredWallIndex(void) {
    return hovered_wall;
}

int Editor_GetHoveredSectorID(void) {
    return hovered_sector;
}

int Editor_GetSelectedSectorID(void) {
    return (sel_type == SEL_SECTOR) ? sel_id : -1;
}

int Editor_GetSelectedWallIndex(void) {
    return (sel_type == SEL_WALL) ? sel_id : -1;
}

int Editor_GetSelectedEntityID(void) {
    return (sel_type == SEL_ENTITY) ? sel_id : -1;
}

int Editor_GetHoveredEntityID(void) {
    return hovered_entity;
}

int Editor_GetHoveredPointIndex(void) {
    return hovered_point;
}

int Editor_GetSelectedPointIndex(void) {
    return (sel_type == SEL_POINT) ? sel_id : -1;
}

float Editor_GetZoom(void) {
    return zoom_level;
}


int Editor_GetGridSize(void) {
    return grid_size;
}

bool Editor_IsDragInvalid(void) {
    return is_dragging && !drag_valid;
}
