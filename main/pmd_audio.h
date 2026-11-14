#pragma once
#include <stdint.h>
#include <stddef.h>

/* PMD .M playback, FM/SSG/Rhythm only (PPZ8/ADPCM intentionally not loaded).
 * All calls must be made with studio_lock() held, except render() which is
 * called by the audio task while it holds the same lock. */
int pmd_audio_load(const char *path);
void pmd_audio_stop(void);
int pmd_audio_active(void);
void pmd_audio_toggle_pause(void);
int pmd_audio_paused(void);
void pmd_audio_render(int16_t *buf, int frames);
const char *pmd_audio_title(void);
size_t pmd_audio_file_size(void);
void pmd_audio_track_snapshot(uint8_t key[10], uint8_t active[10], uint8_t vol[10]);
