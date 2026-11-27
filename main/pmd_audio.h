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
const char *pmd_audio_title_raw(void);
size_t pmd_audio_file_size(void);
void pmd_audio_track_snapshot(uint8_t key[10], uint8_t active[10], uint8_t vol[10]);
#define PMD_ALL_CHANNELS 22
#define PMD_MIX_CHANNELS 19
void pmd_audio_snapshot_all(uint8_t key[PMD_ALL_CHANNELS], uint8_t active[PMD_ALL_CHANNELS], uint8_t vol[PMD_ALL_CHANNELS], uint8_t tone[PMD_ALL_CHANNELS], uint8_t noise[PMD_ALL_CHANNELS]);
void pmd_audio_drum_snapshot(uint8_t active[6], uint8_t level[6]);
void pmd_audio_toggle_channel(int ch);
int pmd_audio_channel_muted(int ch);
