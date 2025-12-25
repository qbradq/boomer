#include "fs.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static mz_zip_archive g_archive;
static bool g_initialized = false;
static bool g_is_directory = false;
static char g_base_path[256];

// User Data State
static char g_user_data_path[256] = {0};
static bool g_user_data_init = false;

static void EnsureDirectory(const char* path) {
#ifdef _WIN32
    // mkdir(path);
#else
    mkdir(path, 0777); 
#endif
}

bool FS_Init(const char* archive_path) {
    if (g_initialized) FS_Shutdown();
    
    // Check if it's a directory
    struct stat st;
    if (stat(archive_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        g_is_directory = true;
        strncpy(g_base_path, archive_path, sizeof(g_base_path) - 1);
        g_initialized = true;
        printf("FS: Mounted directory '%s'\n", archive_path);
        return true;
    }
    
    // Otherwise try zip
    g_is_directory = false;
    memset(&g_archive, 0, sizeof(g_archive));
    
    if (!mz_zip_reader_init_file(&g_archive, archive_path, 0)) {
        printf("FS: Failed to mount '%s' (Not a directory or valid zip)\n", archive_path);
        return false;
    }
    
    g_initialized = true;
    printf("FS: Mounted archive '%s'\n", archive_path);
    return true;
}

void FS_Shutdown(void) {
    if (g_initialized) {
        if (!g_is_directory) {
            mz_zip_reader_end(&g_archive);
        }
        g_initialized = false;
    }
}

void* FS_ReadFile(const char* path, size_t* out_size) {
    if (!g_initialized) return NULL;
    
    if (g_is_directory) {
        // Read from OS file system
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", g_base_path, path);
        
        FILE* f = fopen(full_path, "rb");
        if (!f) {
            printf("FS: File '%s' not found in directory.\n", full_path);
            return NULL;
        }
        
        fseek(f, 0, SEEK_END);
        long length = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        if (length < 0) { fclose(f); return NULL; }
        
        size_t size = (size_t)length;
        void* p = malloc(size + 1);
        if (!p) { fclose(f); return NULL; }
        
        if (fread(p, 1, size, f) != size) {
            free(p);
            fclose(f);
            return NULL;
        }
        
        ((char*)p)[size] = 0; // Null Check
        fclose(f);
        
        if (out_size) *out_size = size;
        return p;

    } else {
        // Read from Zip
        int file_index = mz_zip_reader_locate_file(&g_archive, path, NULL, 0);
        if (file_index < 0) {
            printf("FS: File '%s' not found in archive.\n", path);
            return NULL;
        }
        
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&g_archive, file_index, &stat)) {
            return NULL;
        }
        
        size_t size = (size_t)stat.m_uncomp_size;
        
        void* p = malloc(size + 1);
        if (!p) return NULL;
        
        if (!mz_zip_reader_extract_to_mem(&g_archive, file_index, p, size, 0)) {
            free(p);
            return NULL;
        }
        
        ((char*)p)[size] = 0; // Null Check
        
        if (out_size) *out_size = size;
        return p;
    }
}

void FS_FreeFile(void* data) {
    if (data) free(data);
}

// --- User Data Persistence ---

bool FS_InitUserData(const char* mount_point) {
    strncpy(g_user_data_path, mount_point, sizeof(g_user_data_path) - 1);
    g_user_data_init = true;
    
    printf("FS: User Data Path: %s\n", g_user_data_path);
    
#ifdef __EMSCRIPTEN__
    // Create directory in MEMFS
    EnsureDirectory(g_user_data_path);
    
    // Mount IDBFS to sync with IndexedDB
    // We assume 'mount_point' is a simple directory name like "data"
    // IDBFS needs to be mounted to a directory.
    // NOTE: This ASM block is asynchronous in spirit but synchfs is async.
    // However, Emscripten's FS is synchronous. syncfs is async in JS land.
    // We might need to wait, but usually main loop starts after?
    // Actually, FS.syncfs(true, func) populates MEMFS from DB. 
    EM_ASM({
        var path = UTF8ToString($0);
        
        // Mount IDBFS
        FS.mount(IDBFS, {}, path);
        
        // Sync from DB to Memory
        FS.syncfs(true, function (err) {
            if (err) console.error("IDBFS Sync Error (Load):", err);
            else console.log("IDBFS Loaded successfully.");
        });
    }, mount_point);
    
#else
    EnsureDirectory(g_user_data_path);
#endif

    return true;
}

bool FS_WriteUserData(const char* filename, const void* data, size_t size) {
    if (!g_user_data_init) return false;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", g_user_data_path, filename);
    
    FILE* f = fopen(full_path, "wb");
    if (!f) {
        printf("FS: Failed to open user file for writing: %s\n", full_path);
        return false;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    if (written != size) {
        printf("FS: Failed to write full data to %s\n", full_path);
        return false;
    }
    
#ifdef __EMSCRIPTEN__
    // Sync Memory to DB
    EM_ASM({
        FS.syncfs(false, function (err) {
            if (err) console.error("IDBFS Sync Error (Save):", err);
            // else console.log("IDBFS Saved successfully.");
        });
    });
#endif

    return true;
}

void* FS_ReadUserData(const char* filename, size_t* out_size) {
    if (!g_user_data_init) return NULL;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", g_user_data_path, filename);
    
    FILE* f = fopen(full_path, "rb");
    if (!f) {
        // printf("FS: User file not found: %s\n", full_path);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (length < 0) { fclose(f); return NULL; }
    
    size_t size = (size_t)length;
    void* p = malloc(size + 1);
    if (!p) { fclose(f); return NULL; }
    
    if (fread(p, 1, size, f) != size) {
        free(p);
        fclose(f);
        return NULL;
    }
    
    ((char*)p)[size] = 0;
    fclose(f);
    
    if (out_size) *out_size = size;
    return p;
}
