#ifndef BOOMER_CONFIG_H
#define BOOMER_CONFIG_H

#include "types.h"
#include <stdbool.h>

typedef struct {
    int logical_width;
    int logical_height;
    int window_scale;
    bool fullscreen;
    
    // Console Style
    u32 console_bg_color;   // 0xRRGGBBAA
    u32 console_text_color; // 0xRRGGBBAA
    char console_font_path[64];
    int console_font_size;
} GameConfig;

// Loads config from fs.
// Loads config from fs.
// 1. Loads base_path/config.json
// 2. Loads user_data/config.json (overrides)
bool Config_Load(void);

// Saves current config (including input map) to user_data/config.json
void Config_Save(void);

// Input System
bool Config_IsActionDown(const char* action);
bool Config_IsActionPressed(const char* action);

// Get global config
const GameConfig* Config_Get(void);

#endif // BOOMER_CONFIG_H
