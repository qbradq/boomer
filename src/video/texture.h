#ifndef BOOMER_TEXTURE_H
#define BOOMER_TEXTURE_H

#include "../core/types.h"

typedef i32 TextureID;

typedef struct GameTexture {
    u32 width, height;
    u32 channels;
    u32* pixels; // ABGR/ARGB buffer
} GameTexture;

// Initialize Texture Manager
void Texture_Init(void);
void Texture_Shutdown(void);

// Load texture from FS path. Returns existing ID if already loaded.
// Returns -1 on failure.
TextureID Texture_Load(const char* path);

// Get Texture by ID
GameTexture* Texture_Get(TextureID id);

// Get Texture ID by name (path) if loaded, else -1
TextureID Texture_GetID(const char* name);

// Get Texture Name by ID
const char* Texture_GetName(TextureID id);

#endif // BOOMER_TEXTURE_H
