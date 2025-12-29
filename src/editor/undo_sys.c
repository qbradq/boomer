#include "undo_sys.h"
#include "../world/world.h"
#include "../game/entity.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_UNDO_DEPTH 100

typedef struct {
    Map map;
    EntitySnapshot* entities;
    int entity_count;
} EditorState;

static EditorState undo_stack[MAX_UNDO_DEPTH];
static int undo_cnt = 0;

static EditorState redo_stack[MAX_UNDO_DEPTH];
static int redo_cnt = 0;

static void FreeState(EditorState* state) {
    Map_Free(&state->map);
    if (state->entities) {
        free(state->entities);
        state->entities = NULL;
    }
    state->entity_count = 0;
}

static void CaptureState(EditorState* dest, Map* src_map) {
    // Clone Map
    // Dest map should be empty or freed by caller
    Map_Clone(&dest->map, src_map); // Allocates new memory
    
    // Snapshot Entities
    dest->entity_count = Entity_GetSnapshot(&dest->entities);
}

void Undo_Init(void) {
    Undo_Clear();
}

void Undo_Clear(void) {
    for (int i=0; i<undo_cnt; ++i) FreeState(&undo_stack[i]);
    undo_cnt = 0;
    
    for (int i=0; i<redo_cnt; ++i) FreeState(&redo_stack[i]);
    redo_cnt = 0;
}

void Undo_PushState(Map* map) {
    // 1. Clear Redo Stack
    for (int i=0; i<redo_cnt; ++i) FreeState(&redo_stack[i]);
    redo_cnt = 0;

    // 2. Push to Undo Stack
    if (undo_cnt >= MAX_UNDO_DEPTH) {
        // Full, drop oldest (index 0)
        FreeState(&undo_stack[0]);
        // Shift
        memmove(&undo_stack[0], &undo_stack[1], sizeof(EditorState) * (MAX_UNDO_DEPTH - 1));
        undo_cnt--;
    }
    
    // Capture state
    CaptureState(&undo_stack[undo_cnt], map);
    undo_cnt++;
    
    printf("Undo: Pushed state. Undo Stacks: %d\n", undo_cnt);
}

bool Undo_PerformUndo(Map* map) {
    if (undo_cnt == 0) return false;
    
    // 1. Push Current State to Redo Stack
    if (redo_cnt >= MAX_UNDO_DEPTH) {
        FreeState(&redo_stack[0]);
        memmove(&redo_stack[0], &redo_stack[1], sizeof(EditorState) * (MAX_UNDO_DEPTH - 1));
        redo_cnt--;
    }
    CaptureState(&redo_stack[redo_cnt], map);
    redo_cnt++;
    
    // 2. Pop from Undo Stack
    undo_cnt--;
    EditorState* state = &undo_stack[undo_cnt];
    
    // 3. Restore
    Map_Restore(map, &state->map);
    Entity_Restore(state->entities, state->entity_count);
    
    // 4. Free the popped state (since we copied it to Global)
    // Wait, Map_Restore copies deeply? Yes.
    // Entity_Restore effectively copies (spawns/sets props).
    FreeState(state);
    
    printf("Undo: Performed. Undo: %d, Redo: %d\n", undo_cnt, redo_cnt);
    return true;
}

bool Undo_PerformRedo(Map* map) {
    if (redo_cnt == 0) return false;
    
    // 1. Push Current State to Undo Stack
    if (undo_cnt >= MAX_UNDO_DEPTH) {
        FreeState(&undo_stack[0]);
        memmove(&undo_stack[0], &undo_stack[1], sizeof(EditorState) * (MAX_UNDO_DEPTH - 1));
        undo_cnt--;
    }
    CaptureState(&undo_stack[undo_cnt], map);
    undo_cnt++;
    
    // 2. Pop from Redo Stack
    redo_cnt--;
    EditorState* state = &redo_stack[redo_cnt];
    
    // 3. Restore
    Map_Restore(map, &state->map);
    Entity_Restore(state->entities, state->entity_count);
    
    // 4. Free popped
    FreeState(state);
    
    printf("Redo: Performed. Undo: %d, Redo: %d\n", undo_cnt, redo_cnt);
    return true;
}
