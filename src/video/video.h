#ifndef BOOMER_VIDEO_H
#define BOOMER_VIDEO_H

#include "../core/types.h"
#include <stdbool.h>

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

// draw a vertical line (optimization)
void Video_DrawVertLine(int x, int y0, int y1, Color color);

// Present the framebuffer to the screen
void Video_Present(void);

#endif // BOOMER_VIDEO_H
