/**
 * @file stats.h
 * @brief Cross-core runtime telemetry shared by audio, UI and power paths.
 */#pragma once
extern volatile int g_cpu_pct;
extern volatile int g_underruns;
extern volatile int g_fps;
extern volatile int g_tilt_x;
extern volatile int g_tilt_y;
extern volatile int g_headphones;
extern volatile int g_hp_override;
extern volatile int g_hp_att;
#define APP_SCOPE_N 128
extern int16_t g_audio_scope[APP_SCOPE_N];
extern volatile unsigned g_audio_scope_w;
extern volatile int g_pmd_level[2];
