#ifndef BOOMER_VIDEO_H
#define BOOMER_VIDEO_H

#include "../core/types.h"
#include <stdbool.h>

struct Texture; // Forward declaration
struct GameTexture;

// Video configuration
#define VIDEO_WIDTH  320
#define VIDEO_HEIGHT 180
#define VIDEO_SCALE  4

extern u32* video_pixels;

// Initialize the video system
bool Video_Init(const char* title);

// Shutdown the video system
void Video_Shutdown(void);

// Change window scale (relative to delta, clamped 1-12)
void Video_ChangeScale(int delta);

// Toggle borderless fullscreen with integer scaling
void Video_ToggleFullscreen(void);

// Clear the framebuffer with a specific color
void Video_Clear(Color color);

// Set a pixel in the framebuffer
void Video_PutPixel(int x, int y, Color color);

// Draw a line
void Video_DrawLine(int x0, int y0, int x1, int y1, Color color);

// Draw a vertical line
void Video_DrawVertLine(int x, int y1, int y2, Color color);

// Draw a textured column
void Video_DrawTexturedColumn(int x, int y_start, int y_end, struct GameTexture* tex, int tex_x, float v_start, float v_step);

// --- Advanced Rendering Pipeline (For Editor) ---
void Video_BeginFrame(void);
void Video_DrawGame(void* dst_rect); // If NULL, fills screen/window
void Video_EndFrame(void);

// Present the framebuffer to the screen
void Video_Present(void);

#endif // BOOMER_VIDEO_H
