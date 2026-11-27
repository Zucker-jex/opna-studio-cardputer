#include "pmd_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>

#include "pfm/player.h"

static const char *TAG = "pmd";
static pfm_player *s_player;
static uint8_t *s_file;
static size_t s_len;
static int s_active;
static int s_paused;
static unsigned s_mask;
static int s_ppz8_mute;
static char s_title[96];
static char s_title_raw[128];

static void clean_title(const char *src, char *dst, size_t cap) {
  size_t j = 0;
  if (!src) src = "";
  for (size_t i = 0; src[i] && j + 1 < cap; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c >= 0x20 && c < 0x7f) dst[j++] = (char)c;
    else if (j && dst[j - 1] != '?') dst[j++] = '?';
  }
  while (j && dst[j - 1] == ' ') j--;
  dst[j] = 0;
}

int pmd_audio_load(const char *path) {
  pmd_audio_stop();

  FILE *f = fopen(path, "rb");
  if (!f) {
    ESP_LOGE(TAG, "open failed: %s", path);
    return -1;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0 || n > 0xffff) {
    fclose(f);
    ESP_LOGE(TAG, "invalid PMD size: %ld", n);
    return -2;
  }

  uint8_t *buf = (uint8_t *)malloc((size_t)n);
  if (!buf) {
    fclose(f);
    ESP_LOGE(TAG, "out of memory for %ld-byte PMD", n);
    return -3;
  }
  size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  if (got != (size_t)n) {
    free(buf);
    return -1;
  }

  if (!s_player) s_player = pfm_player_instance();
  pfm_player_init(s_player);
  if (!pfm_player_load(s_player, buf, (size_t)n)) {
    free(buf);
    ESP_LOGE(TAG, "not a valid PMD file: %s", path);
    return -4;
  }

  s_file = buf;
  s_len = (size_t)n;
  s_active = 1;
  s_paused = 0;
  s_mask = 0;
  s_ppz8_mute = 0;
  pfm_player_set_mask(s_player, 0, 0);
  const char *raw_title = pfm_player_get_title(s_player);
  strncpy(s_title_raw, raw_title ? raw_title : "", sizeof(s_title_raw) - 1);
  s_title_raw[sizeof(s_title_raw) - 1] = 0;
  clean_title(raw_title, s_title, sizeof(s_title));
  if (!s_title[0]) {
    const char *base = strrchr(path, '/');
    snprintf(s_title, sizeof(s_title), "%s", base ? base + 1 : path);
  }
  if (!s_title_raw[0]) {
    const char *base = strrchr(path, '/');
    snprintf(s_title_raw, sizeof(s_title_raw), "%s", base ? base + 1 : path);
  }
  ESP_LOGI(TAG, "loaded %s (%u B): %s", path, (unsigned)s_len, s_title);
  return 0;
}

void pmd_audio_stop(void) {
  s_active = 0;
  s_paused = 0;
  s_mask = 0;
  if (s_file) {
    free(s_file);
    s_file = nullptr;
  }
  s_len = 0;
  s_title[0] = 0;
  s_title_raw[0] = 0;
}

int pmd_audio_active(void) { return s_active; }

void pmd_audio_render(int16_t *buf, int frames) {
  if (!s_active || !s_player) return;
  if (s_paused) {
    memset(buf, 0, (size_t)frames * 2 * sizeof(int16_t));
    return;
  }
  pfm_player_render(s_player, buf, (size_t)frames);
}

void pmd_audio_toggle_pause(void) {
  if (s_active) s_paused = !s_paused;
}
int pmd_audio_paused(void) { return s_paused; }

/* MIX display order: 0-8 FM1..FM6/FM3B..D, 9-10 SSG1/2,
   11-12 SSG3-T/N (one physical SSG3), 13-18 BD/SD/TOP/HH/TOM/RIM. */
static unsigned mask_bit(int ch) {
  if (ch < 3) return 1u << ch;
  if (ch < 6) return 1u << 2;
  if (ch < 9) return 1u << (ch - 3);
  if (ch < 11) return 1u << (ch - 3);  /* 9->6, 10->7 */
  if (ch < 13) return 1u << 8;          /* SSG3-T/N */
  return 1u << (9 + (ch - 13));         /* 13..18 -> bits 9..14 */
}

void pmd_audio_toggle_channel(int ch) {
  if (ch < 0 || ch >= PMD_MIX_CHANNELS || !s_player) return;
  s_mask ^= mask_bit(ch);
  pfm_player_set_mask(s_player, s_mask, 0);
}

int pmd_audio_channel_muted(int ch) {
  if (ch < 0 || ch >= PMD_MIX_CHANNELS) return 0;
  return (s_mask & mask_bit(ch)) ? 1 : 0;
}

const char *pmd_audio_title(void) { return s_title; }
const char *pmd_audio_title_raw(void) { return s_title_raw; }
size_t pmd_audio_file_size(void) { return s_len; }

void pmd_audio_track_snapshot(uint8_t key[10], uint8_t active[10], uint8_t vol[10]) {
  for (int i = 0; i < 10; i++) { key[i] = 0; active[i] = 0; vol[i] = 0; }
  if (!s_active || !s_player) return;
  struct pmd_track_view v[10];
  pfm_player_track_snapshot(s_player, v);
  for (int i = 0; i < 10; i++) {
    key[i] = v[i].key;
    active[i] = v[i].playing;
    vol[i] = v[i].volume;
  }
}

void pmd_audio_drum_snapshot(uint8_t active[6], uint8_t level[6]) {
  for (int i = 0; i < 6; i++) { active[i] = 0; level[i] = 0; }
  if (!s_active || !s_player) return;
  pfm_player_drum_snapshot(s_player, active, level);
}

void pmd_audio_snapshot_all(uint8_t key[PMD_ALL_CHANNELS], uint8_t active[PMD_ALL_CHANNELS], uint8_t vol[PMD_ALL_CHANNELS], uint8_t tone[PMD_ALL_CHANNELS], uint8_t noise[PMD_ALL_CHANNELS]) {
  for (int i = 0; i < PMD_ALL_CHANNELS; i++) { key[i] = 0; active[i] = 0; vol[i] = 0; tone[i] = 0; noise[i] = 0; }
  if (!s_active || !s_player) return;
  struct pmd_track_view v[PMD_ALL_CHANNELS];
  pfm_player_track_snapshot_all(s_player, v);
  for (int i = 0; i < PMD_ALL_CHANNELS; i++) {
    key[i] = v[i].key;
    active[i] = v[i].playing;
    vol[i] = v[i].volume;
    tone[i] = v[i].tone;
    noise[i] = v[i].noise;
  }
}
