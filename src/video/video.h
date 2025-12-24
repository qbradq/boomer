#ifndef BOOMER_VIDEO_H
#define BOOMER_VIDEO_H

#include "../core/types.h"
#include <stdbool.h>

typedef struct Texture Texture;

// Video configuration
#define VIDEO_WIDTH  320
#define VIDEO_HEIGHT 200
#define VIDEO_SCALE  4

// Initialize the video system
bool Video_Init(const char* title);

// Shutdown the video system
void Video_Shutdown(void);

// Clear the framebuffer with a specific color
void Video_Clear(Color color);

// draw a pixel to the framebuffer
void Video_PutPixel(int x, int y, Color color);

// draw a line to the framebuffer
void Video_DrawLine(int x0, int y0, int x1, int y1, Color color);

// Draw Vertical Line (Solid Color)
void Video_DrawVertLine(int x, int y1, int y2, Color color);

// Draw Textured Vertical Column
// tex_x: U coordinate (integer 0..width-1)
// v_start: V coordinate at y_start
// v_step: How much V advances per screen pixel
void Video_DrawTexturedColumn(int x, int y_start, int y_end, Texture* tex, int tex_x, float v_start, float v_step);

// Present the framebuffer to the screen
void Video_Present(void);

#endif // BOOMER_VIDEO_H
