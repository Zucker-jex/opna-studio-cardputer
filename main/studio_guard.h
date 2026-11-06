#pragma once
#include <stdint.h>

extern volatile int g_volume;      /* 0..10 */
extern volatile int g_backlight;   /* 0..255 */

void studio_guard_init(void);
void studio_lock(void);
void studio_unlock(void);
