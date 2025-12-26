#include "texture.h"
#include "../core/fs.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TEXTURES 256

typedef struct {
    char name[64];
    GameTexture tex;
    bool active;
} TextureSlot;

static TextureSlot g_textures[MAX_TEXTURES];

void Texture_Init(void) {
    memset(g_textures, 0, sizeof(g_textures));
}

void Texture_Shutdown(void) {
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        if (g_textures[i].active && g_textures[i].tex.pixels) {
            MemFree(g_textures[i].tex.pixels); // Raylib allocator
            g_textures[i].tex.pixels = NULL;
            g_textures[i].active = false;
        }
    }
}

TextureID Texture_Load(const char* path) {
    // 1. Check if already loaded
    TextureID existing = Texture_GetID(path);
    if (existing != -1) return existing;
    
    // 2. Find empty slot
    int slot = -1;
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        if (!g_textures[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        printf("Texture: Max textures reached!\n");
        return -1;
    }
    
    // 3. Load from FS
    size_t size;
    void* data = FS_ReadFile(path, &size);
    if (!data) {
        // printf("Texture: Failed to read file '%s'\n", path);
        return -1;
    }
    
    // Use Raylib to load image from memory
    // Get extension
    const char* ext = strrchr(path, '.');
    if (!ext) ext = ".png"; // default
    
    Image img = LoadImageFromMemory(ext, data, (int)size);
    FS_FreeFile(data);
    
    if (img.data == NULL) {
        printf("Texture: Raylib failed to load '%s'\n", path);
        return -1;
    }
    
    // Ensure RGBA 32 bit
    if (img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    
    // 4. Store
    TextureSlot* s = &g_textures[slot];
    strncpy(s->name, path, sizeof(s->name) - 1);
    s->tex.width = (u32)img.width;
    s->tex.height = (u32)img.height;
    s->tex.channels = 4;
    s->tex.pixels = (u32*)img.data; // We take ownership of img.data
    s->active = true;
    
    // Note: We do NOT UnloadImage(img) because we stole the pointer img.data
    // Raylib's UnloadImage simply frees img.data.
    // We will free it in Texture_Shutdown.
    
    printf("Texture: Loaded '%s' (%dx%d)\n", path, img.width, img.height);
    return slot;
}

GameTexture* Texture_Get(TextureID id) {
    if (id < 0 || id >= MAX_TEXTURES) return NULL;
    if (!g_textures[id].active) return NULL;
    return &g_textures[id].tex;
}

TextureID Texture_GetID(const char* name) {
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        if (g_textures[i].active && strncmp(g_textures[i].name, name, 64) == 0) {
            return i;
        }
    }
    return -1;
}

const char* Texture_GetName(TextureID id) {
    if (id < 0 || id >= MAX_TEXTURES) return "Invalid";
    if (!g_textures[id].active) return "Empty";
    return g_textures[id].name;
}
