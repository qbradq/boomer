#ifndef BOOMER_FS_H
#define BOOMER_FS_H

#include "types.h"
#include <stddef.h>

// Initialize the File System with a main archive (PAK)
bool FS_Init(const char* archive_path);

// Shutdown and close archive
void FS_Shutdown(void);

// Read a file entirely into memory
// Returns pointer to data (null-terminated if text, but check size)
// Returns NULL if not found.
// Caller must call FS_FreeFile.
void* FS_ReadFile(const char* path, size_t* out_size);

// Free file memory
void FS_FreeFile(void* data);

#endif // BOOMER_FS_H
