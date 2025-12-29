#ifndef UNDO_SYS_H
#define UNDO_SYS_H

#include "../world/world_types.h"

// Initialize the undo system
void Undo_Init(void);

// Push the current state onto the undo stack.
// Call this BEFORE performing a destructive action.
void Undo_PushState(Map* map);

// Perform Undo: restores state from undo stack to map
// Returns true if undo was performed
bool Undo_PerformUndo(Map* map);

// Perform Redo: restores state from redo stack to map
// Returns true if redo was performed
bool Undo_PerformRedo(Map* map);

// Clear all history
void Undo_Clear(void);

#endif
