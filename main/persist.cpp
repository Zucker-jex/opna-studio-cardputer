/**
 * @file persist.cpp
 * @brief Song/settings persistence on /sdcard/opna2.p2s.
 *
 * V4 (current): 16 patterns + per-pattern length, pages, volume/backlight,
 *               HP gain, BPM/swing/chain/pattern.
 * V3: 16 patterns without lengths (migrated with length=16).
 * V2: P2 4-pattern format (migrated with pages=4).
 * All writes happen under the engine lock held by the caller.
 */#include "persist.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <esp_log.h>

#include "studio.h"
#include "stats.h"

#define FILE_PATH "/sdcard/opna2.p2s"
#define MAGIC "OPN2"
#define VER 4

static const char *TAG = "persist";

struct st_song_v2 {
  struct st_step step[4][ST_TRACKS][ST_STEPS];
  uint8_t mute[ST_TRACKS];
  uint8_t level[ST_TRACKS];
  uint8_t patch[6];
};

struct st_song_v3 {
  struct st_step step[ST_PATTERNS][ST_TRACKS][ST_STEPS];
  uint8_t mute[ST_TRACKS];
  uint8_t level[ST_TRACKS];
  uint8_t patch[6];
};

struct st_file {
  char magic[4];
  uint8_t ver;
  uint8_t bpm, swing, chain, pat;
  uint8_t vol, bl, out, pages;
  uint16_t song_len;
  struct st_song song;
  uint16_t sum;
} __attribute__((packed));

struct st_file_v3 {
  char magic[4];
  uint8_t ver;
  uint8_t bpm, swing, chain, pat;
  uint8_t vol, bl, out, reserved;
  uint16_t song_len;
  struct st_song_v3 song;
  uint16_t sum;
} __attribute__((packed));

struct st_file_v2 {
  char magic[4];
  uint8_t ver;
  uint8_t bpm, swing, chain, pat;
  uint8_t vol, bl, out, reserved;
  uint16_t song_len;
  struct st_song_v2 song;
  uint16_t sum;
} __attribute__((packed));

struct st_file_hdr {
  char magic[4];
  uint8_t ver;
  uint8_t bpm, swing, chain, pat;
  uint8_t vol, bl, out, pages;
  uint16_t song_len;
} __attribute__((packed));

static struct st_file s_file;
static struct st_file_v3 s_file_v3;
static struct st_file_v2 s_file_v2;

static uint16_t sum_bytes(const uint8_t *p, size_t n) {
  uint32_t s = 0;
  for (size_t i = 0; i < n; i++) s += p[i];
  return (uint16_t)s;
}

int persist_save(int volume, int backlight) {
  memset(&s_file, 0, sizeof(s_file));
  memcpy(s_file.magic, MAGIC, 4);
  s_file.ver = VER;
  s_file.bpm = (uint8_t)studio_bpm();
  s_file.swing = (uint8_t)studio_swing();
  s_file.chain = (uint8_t)(studio_chain() ? 1 : 0);
  s_file.pat = (uint8_t)studio_pattern();
  s_file.vol = (uint8_t)volume;
  s_file.bl = (uint8_t)backlight;
  s_file.pages = (uint8_t)studio_pattern_limit();
  s_file.song_len = (uint16_t)sizeof(struct st_song);
  memcpy(&s_file.song, studio_song(), sizeof(s_file.song));
  s_file.sum = sum_bytes((const uint8_t *)&s_file, offsetof(struct st_file, sum));

  FILE *fp = fopen(FILE_PATH, "wb");
  if (!fp) {
    ESP_LOGE(TAG, "save open failed path=%s errno=%d", FILE_PATH, errno);
    return -1;
  }
  size_t n = fwrite(&s_file, 1, sizeof(s_file), fp);
  int fr = fflush(fp);
  int fc = fclose(fp);
  ESP_LOGI(TAG, "save v4 %s: wrote %u/%u flush=%d close=%d", FILE_PATH,
           (unsigned)n, (unsigned)sizeof(s_file), fr, fc);
  return (n == sizeof(s_file) && fr == 0 && fc == 0) ? 1 : -1;
}

static void apply_v2(void) {
  struct st_song *dst = studio_song();
  memset(dst->step, 0xFF, sizeof(dst->step));
  memcpy(dst->step, s_file_v2.song.step, sizeof(s_file_v2.song.step));
  memcpy(dst->mute, s_file_v2.song.mute, sizeof(dst->mute));
  memcpy(dst->level, s_file_v2.song.level, sizeof(dst->level));
  memcpy(dst->patch, s_file_v2.song.patch, sizeof(dst->patch));
  memset(dst->length, ST_STEPS, sizeof(dst->length));
}
static void apply_v3(void) {
  struct st_song *dst = studio_song();
  memcpy(dst->step, s_file_v3.song.step, sizeof(dst->step));
  memcpy(dst->mute, s_file_v3.song.mute, sizeof(dst->mute));
  memcpy(dst->level, s_file_v3.song.level, sizeof(dst->level));
  memcpy(dst->patch, s_file_v3.song.patch, sizeof(dst->patch));
  memset(dst->length, ST_STEPS, sizeof(dst->length));
}

int persist_load(int *volume, int *backlight) {
  FILE *fp = fopen(FILE_PATH, "rb");
  if (!fp) {
    ESP_LOGW(TAG, "load open failed path=%s errno=%d", FILE_PATH, errno);
    return 0;
  }
  struct st_file_hdr h;
  size_t hn = fread(&h, 1, sizeof(h), fp);
  if (hn != sizeof(h) || memcmp(h.magic, MAGIC, 4) != 0) {
    fclose(fp);
    return -1;
  }
  fseek(fp, 0, SEEK_SET);
  int ok = -1;

  if (h.ver == VER) {
    size_t n = fread(&s_file, 1, sizeof(s_file), fp);
    if (n == sizeof(s_file) && s_file.song_len == sizeof(struct st_song) &&
        s_file.sum == sum_bytes((const uint8_t *)&s_file, offsetof(struct st_file, sum))) {
      memcpy(studio_song(), &s_file.song, sizeof(s_file.song));
      int pages = s_file.pages;
      if (pages < 1 || pages > ST_PATTERNS) pages = ST_PATTERNS;
      studio_set_pattern_limit(pages);
      studio_sync_song();
      studio_set_bpm(s_file.bpm);
      studio_set_swing(s_file.swing);
      studio_set_chain(s_file.chain);
      studio_select_pattern(s_file.pat);
      if (volume) *volume = s_file.vol > 10 ? 10 : s_file.vol;
      if (backlight) *backlight = s_file.bl;
      if (s_file.out >= 1 && s_file.out <= 100) g_hp_att = s_file.out;
      ok = 1;
    }
    ESP_LOGI(TAG, "load v4 %s: read %u/%u", FILE_PATH, (unsigned)n,
             (unsigned)sizeof(s_file));
  } else if (h.ver == 3) {
    size_t n = fread(&s_file_v3, 1, sizeof(s_file_v3), fp);
    if (n == sizeof(s_file_v3) && s_file_v3.song_len == sizeof(struct st_song_v3) &&
        s_file_v3.sum == sum_bytes((const uint8_t *)&s_file_v3,
                                   offsetof(struct st_file_v3, sum))) {
      apply_v3();
      int pages = s_file_v3.reserved;
      if (pages < 1 || pages > ST_PATTERNS) pages = ST_PATTERNS;
      studio_set_pattern_limit(pages);
      studio_sync_song();
      studio_set_bpm(s_file_v3.bpm);
      studio_set_swing(s_file_v3.swing);
      studio_set_chain(s_file_v3.chain);
      studio_select_pattern(s_file_v3.pat);
      if (volume) *volume = s_file_v3.vol > 10 ? 10 : s_file_v3.vol;
      if (backlight) *backlight = s_file_v3.bl;
      if (s_file_v3.out >= 1 && s_file_v3.out <= 100) g_hp_att = s_file_v3.out;
      ok = 1;
    }
    ESP_LOGI(TAG, "load v3 %s: read %u/%u", FILE_PATH, (unsigned)n,
             (unsigned)sizeof(s_file_v3));
  } else if (h.ver == 1 || h.ver == 2) {
    size_t n = fread(&s_file_v2, 1, sizeof(s_file_v2), fp);
    if (n == sizeof(s_file_v2) && s_file_v2.song_len == sizeof(struct st_song_v2) &&
        s_file_v2.sum == sum_bytes((const uint8_t *)&s_file_v2,
                                   offsetof(struct st_file_v2, sum))) {
      apply_v2();
      studio_set_pattern_limit(4);
      studio_sync_song();
      studio_set_bpm(s_file_v2.bpm);
      studio_set_swing(s_file_v2.swing);
      studio_set_chain(s_file_v2.chain);
      studio_select_pattern(s_file_v2.pat);
      if (volume) *volume = s_file_v2.vol > 10 ? 10 : s_file_v2.vol;
      if (backlight) *backlight = s_file_v2.bl;
      if (s_file_v2.out >= 1 && s_file_v2.out <= 100) g_hp_att = s_file_v2.out;
      ok = 1;
    }
    ESP_LOGI(TAG, "load v%d %s: read %u/%u", h.ver, FILE_PATH, (unsigned)n,
             (unsigned)sizeof(s_file_v2));
  }
  fclose(fp);
  return ok;
}
