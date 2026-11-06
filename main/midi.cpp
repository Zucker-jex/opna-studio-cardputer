/**
 * @file midi.cpp
 * @brief Streaming Standard MIDI File (SMF format 0) exporter.
 *
 * Events are written in pattern/step order; note-offs are kept in a tiny
 * per-step table and sorted before writing, so no large event buffer or
 * dynamic allocation is needed.
 */#include "midi.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>

#include "studio.h"

#define PPQ 480
#define TICK16 120

static void put32(FILE *f, uint32_t v) {
  fputc((v >> 24) & 255, f); fputc((v >> 16) & 255, f);
  fputc((v >> 8) & 255, f); fputc(v & 255, f);
}
static void put16(FILE *f, uint16_t v) {
  fputc((v >> 8) & 255, f); fputc(v & 255, f);
}
static void putvar(FILE *f, uint32_t v) {
  uint8_t b[4];
  int n = 0;
  b[n++] = v & 0x7f;
  while ((v >>= 7) != 0) b[n++] = (v & 0x7f) | 0x80;
  while (n--) fputc(b[n], f);
}

struct off {
  uint32_t tick;
  uint8_t ch, note;
};

int midi_export(const char *path) {
  const struct st_song *song = studio_song();
  int pages = studio_pattern_limit();
  int bpm = studio_bpm();
  if (bpm < 1) bpm = 1;

  FILE *f = fopen(path, "wb");
  if (!f) {
    ESP_LOGE("midi", "open %s failed", path);
    return -1;
  }
  fwrite("MThd", 1, 4, f); put32(f, 6); put16(f, 0); put16(f, 1); put16(f, PPQ);
  fwrite("MTrk", 1, 4, f);
  long lenpos = ftell(f);
  put32(f, 0);

  /* tempo */
  putvar(f, 0); fputc(0xff, f); fputc(0x51, f); fputc(3, f);
  uint32_t usec = 60000000u / (uint32_t)bpm;
  fputc((usec >> 16) & 255, f); fputc((usec >> 8) & 255, f); fputc(usec & 255, f);

  uint32_t last = 0;
  uint32_t at = 0;
  int notes = 0;

  for (int p = 0; p < pages; p++) {
    int len = studio_pattern_length(p);
    for (int s = 0; s < len; s++) {
      uint32_t base = at + (uint32_t)s * TICK16;
      struct off offs[ST_TRACKS];
      int noff = 0;
      int first = 1;

      for (int t = 0; t < ST_TRACKS; t++) {
        const struct st_step *st = &song->step[p][t][s];
        if (st->note == 0xff || song->mute[t] || song->level[t] == 0) continue;
        int ch, note = st->note;
        if (t < 6) ch = t;
        else if (t == 6) ch = 6;
        else if (t == 7) ch = 7;
        else if (t == 8) ch = 8;
        else {
          static const uint8_t drum_note[6] = {36, 38, 39, 42, 45, 47};
          ch = 9;
          note = drum_note[note <= 5 ? note : 0];
        }
        int vel = st->vel * 8;
        if (vel < 1) vel = 1;
        if (vel > 127) vel = 127;

        putvar(f, first ? base - last : 0);
        first = 0;
        last = base;
        fputc(0x90 | ch, f); fputc(note, f); fputc(vel, f);
        notes++;

        uint32_t g = (uint32_t)TICK16 * (st->gate + 1) / 16u;
        if (g < 8) g = 8;
        offs[noff].tick = base + g;
        offs[noff].ch = (uint8_t)ch;
        offs[noff].note = (uint8_t)note;
        noff++;
      }

      /* sort the at-most-10 note-offs by time */
      for (int i = 1; i < noff; i++) {
        struct off o = offs[i];
        int j = i - 1;
        while (j >= 0 && offs[j].tick > o.tick) {
          offs[j + 1] = offs[j];
          j--;
        }
        offs[j + 1] = o;
      }
      for (int i = 0; i < noff; i++) {
        putvar(f, offs[i].tick - last);
        last = offs[i].tick;
        fputc(0x80 | offs[i].ch, f);
        fputc(offs[i].note, f); fputc(0, f);
      }
    }
    at += (uint32_t)len * TICK16;
  }

  putvar(f, at > last ? at - last : 0);
  fputc(0xff, f); fputc(0x2f, f); fputc(0, f);
  long end = ftell(f);
  uint32_t mlen = (uint32_t)(end - lenpos - 4);
  fseek(f, lenpos, SEEK_SET);
  put32(f, mlen);
  fseek(f, 0, SEEK_END);
  fclose(f);
  ESP_LOGI("midi", "exported %s: %d notes, %u ticks", path, notes, at);
  return notes > 0 ? 1 : -1;
}
