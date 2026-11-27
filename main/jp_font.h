#pragma once
#include <stdint.h>
#include <M5GFX.h>

/* Draw a CP932 (Shift-JIS) title right-aligned at `right`, using the
 * generated 12x12 Fusion Pixel bitmap table. */
void jp_draw_title_right(lgfx::LovyanGFX *d, int right, int y,
                         const char *cp932, uint16_t color);
int jp_has_wide(const char *cp932);
void jp_draw_title_header(lgfx::LovyanGFX *d, int right, int y,
                          const char *cp932, uint16_t color);
