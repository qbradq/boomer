#include "texture.h"
#include "../core/fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO // We use memory buffers
// #define STBI_ONLY_PNG
#include "stb_image.h"

#define MAX_TEXTURES 256

typedef struct {
    char name[64];
    Texture tex;
    bool active;
} TextureSlot;

static TextureSlot g_textures[MAX_TEXTURES];

void Texture_Init(void) {
    memset(g_textures, 0, sizeof(g_textures));
}

void Texture_Shutdown(void) {
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        if (g_textures[i].active && g_textures[i].tex.pixels) {
            stbi_image_free(g_textures[i].tex.pixels);
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
    
    int w, h, channels;
    // Force 4 channels (RGBA)
    stbi_uc* pixels = stbi_load_from_memory((stbi_uc*)data, (int)size, &w, &h, &channels, 4);
    FS_FreeFile(data);
    
    if (!pixels) {
        printf("Texture: STB failed to load '%s': %s\n", path, stbi_failure_reason());
        return -1;
    }
    
    // 4. Store
    TextureSlot* s = &g_textures[slot];
    strncpy(s->name, path, sizeof(s->name) - 1);
    s->tex.width = (u32)w;
    s->tex.height = (u32)h;
    s->tex.channels = 4;
    s->tex.pixels = (u32*)pixels; // Cast directly, assuming system endianness matches or we handle it later
    s->active = true;
    
    printf("Texture: Loaded '%s' (%dx%d)\n", path, w, h);
    return slot;
}

Texture* Texture_Get(TextureID id) {
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
