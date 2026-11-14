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
static char s_title[96];

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
  clean_title(pfm_player_get_title(s_player), s_title, sizeof(s_title));
  if (!s_title[0]) {
    const char *base = strrchr(path, '/');
    snprintf(s_title, sizeof(s_title), "%s", base ? base + 1 : path);
  }
  ESP_LOGI(TAG, "loaded %s (%u B): %s", path, (unsigned)s_len, s_title);
  return 0;
}

void pmd_audio_stop(void) {
  s_active = 0;
  s_paused = 0;
  if (s_file) {
    free(s_file);
    s_file = nullptr;
  }
  s_len = 0;
  s_title[0] = 0;
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

const char *pmd_audio_title(void) { return s_title; }
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
