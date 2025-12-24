#include "fs.h"
#include "miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static mz_zip_archive g_archive;
static bool g_initialized = false;
static bool g_is_directory = false;
static char g_base_path[256];

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
