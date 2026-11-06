/**
 * @file ui.cpp
 * @brief Dual-screen UI and input dispatch.
 *
 * Rendering model:
 *   - external 320x240 M5Canvas, redrawn on key/page or every 200 ms;
 *   - internal 240x135 M5Canvas HUD, refreshed every 50 ms;
 *   - both canvases live in internal SRAM (no PSRAM on the ADV).
 *
 * Input model: Tab/Backspace switch pages, Fn is the MOD modifier, and the
 * punctuation keys ; . , / are the only arrow keys. SYN letters/digits are
 * consumed as piano keys so WASD never becomes navigation there.
 */#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <esp_timer.h>
#include <esp_log.h>

#include "studio.h"
#include "patches.h"
#include "studio_guard.h"
#include "persist.h"
#include "midi.h"
#include "stats.h"
#include <M5Unified.hpp>

static lgfx::LovyanGFX *s_ext = nullptr;
static lgfx::LovyanGFX *s_int = nullptr;
static M5Canvas *s_ext_cv = nullptr;
static M5Canvas *s_hud_cv = nullptr;
static bool s_sd_ok = false;

static uint16_t C_BG, C_PANEL, C_PANEL2, C_LINE, C_TEXT, C_DIM, C_DIMMER;
static uint16_t C_GOLD, C_CYAN, C_GREEN, C_RED, C_ORANGE, C_PURPLE, C_WHITE;
static uint16_t C_BG2, C_YELLOW;
static uint16_t s_trk[10];
static uint16_t s_page_accent[5];

static uint16_t trk_col(int t) { return s_trk[(t < 0 || t > 9) ? 0 : t]; }

static const char *const k_page[5] = { "SEQ", "SYN", "MIX", "VIS", "SYS" };
static int s_page = 0;
static int s_seq_track = 0, s_seq_step = 0;
static int s_seq_pencil = 60;
static int clampi(int v, int lo, int hi);
static char s_msg[40];
static int s_syn_patch = 0, s_syn_key = 0, s_syn_octave = 0;
static int s_mix_sel = 0;
enum {
  SYS_BPM, SYS_SWING, SYS_CHAIN, SYS_PATTERN, SYS_PAGES, SYS_LENGTH,
  SYS_COPY, SYS_CLEAR, SYS_OUTPUT, SYS_VOLUME, SYS_HP_GAIN, SYS_BACKLIGHT,
  SYS_SAVE, SYS_LOAD, SYS_EXPORT, SYS_DEMO, SYS_SONG,
  SYS_BATTERY, SYS_TILT, SYS_ENGINE, SYS_SHUTDOWN, SYS_ROWS
};
static int s_sys_sel = 0;
static int s_fn_held = 0;
static int s_vis_scope = 0;
static int s_sys_top = 0;
static uint32_t s_fps_t = 0;
static int s_fps_n = 0;
static bool s_mod = false;

static bool s_force_ext = true, s_force_hud = true;
static uint32_t s_last_ext = 0, s_last_hud = 0;

static void set_font(lgfx::LovyanGFX *d, int size) {
  d->setFont(&lgfx::fonts::Font0);
  d->setTextSize(size);
  d->setTextDatum(textdatum_t::top_left);
  d->setTextWrap(false, false);
}

static void text(lgfx::LovyanGFX *d, int x, int y, const char *s, uint16_t fg) {
  d->setTextColor(fg, C_BG);
  d->drawString(s, x, y);
}

static void text_bg(lgfx::LovyanGFX *d, int x, int y, const char *s, uint16_t fg,
                    uint16_t bg) {
  d->setTextColor(fg, bg);
  d->drawString(s, x, y);
}

static void text_r(lgfx::LovyanGFX *d, int xr, int y, const char *s, uint16_t fg) {
  d->setTextColor(fg, C_BG);
  d->drawRightString(s, xr, y);
}

static void set_page(int p) {
  if (p < 0) p = 0;
  if (p > 4) p = 4;
  if (s_page == 1 && p != 1) {
    s_seq_pencil = clampi(60 + s_syn_octave * 12 + s_syn_key, 0, 127);
  }
  s_page = p;
  studio_lock();
  studio_set_analysis(p == 3 ? 2 : (p == 2 ? 1 : 0));
  studio_live_focus(p == 1);
  if (p == 1) {
    if (s_seq_track < 6) s_syn_patch = studio_song()->patch[s_seq_track] % ST_PATCHES;
    studio_live_patch(s_syn_patch);
  } else {
    studio_live_off();
  }
  studio_unlock();
  s_force_ext = s_force_hud = true;
}

static void draw_status(lgfx::LovyanGFX *d) {
  d->fillRect(0, 0, 320, 18, C_PANEL);
  set_font(d, 1);
  text_bg(d, 4, 5, "OPNA STUDIO", C_GOLD, C_PANEL);
  text_bg(d, 100, 5, studio_playing() ? "> PLAY" : "| STOP",
          studio_playing() ? C_GREEN : C_DIMMER, C_PANEL);
  char b[32];
  snprintf(b, sizeof(b), "BPM %d  PAT %c", studio_bpm(),
           'A' + studio_pattern());
  text_bg(d, 160, 5, b, C_CYAN, C_PANEL);
  text_bg(d, 286, 5, s_sd_ok ? "SD" : "NO", s_sd_ok ? C_GREEN : C_RED, C_PANEL);
  if (s_mod) text_bg(d, 252, 5, "MOD", C_GOLD, C_PANEL);
  d->fillRect(0, 17, 320, 1, s_page_accent[s_page]);
}

static void draw_header(lgfx::LovyanGFX *d) {
  d->fillRect(0, 18, 320, 30, C_PANEL2);
  set_font(d, 2);
  text_bg(d, 6, 22, k_page[s_page], s_page_accent[s_page], C_PANEL2);
  set_font(d, 1);
  char b[64];
  if (s_page == 3) snprintf(b, sizeof(b), s_vis_scope ? "SCOPE" : "SPECTRUM");
  else if (s_page == 4) snprintf(b, sizeof(b), "SYSTEM");
  else if (s_page == 2) snprintf(b, sizeof(b), "MIXER / LEVELS");
  else if (s_page == 1) snprintf(b, sizeof(b), "LIVE FM6 / ARP");
  else snprintf(b, sizeof(b), "PATTERN %c  CHAIN %s", 'A' + studio_pattern(),
                studio_chain() ? "ON" : "OFF");
  text_r(d, 314, 24, b, C_DIM);
  d->fillRect(0, 47, 320, 1, C_LINE);
}

static void draw_tabs(lgfx::LovyanGFX *d) {
  d->fillRect(0, 224, 320, 16, C_BG);
  for (int i = 0; i < 5; i++) {
    int x = i * 64;
    bool sel = (i == s_page);
    if (sel) d->fillRect(x, 224, 64, 16, C_PANEL2);
    d->fillRect(x, 224, 64, 2, sel ? s_page_accent[i] : C_LINE);
    set_font(d, 1);
    text_bg(d, x + 20, 230, k_page[i], sel ? C_WHITE : C_DIM,
            sel ? C_PANEL2 : C_BG);
  }
}

static void draw_hint(lgfx::LovyanGFX *d, const char *s) {
  d->fillRect(0, 207, 320, 17, C_PANEL);
  set_font(d, 1);
  text_bg(d, 6, 211, s, C_DIMMER, C_PANEL);
}

static void draw_seq(lgfx::LovyanGFX *d) {
  struct st_song *song = studio_song();
  int pat = studio_pattern();
  int cur = studio_playing() ? studio_step() : -1;
  int len = studio_pattern_length(pat);

  d->fillRect(0, 48, 320, 159, C_BG);
  set_font(d, 1);
  char b[64];
  text(d, 6, 51, "PAT", C_DIM);
  for (int i = 0; i < ST_PATTERNS; i++) {
    char c = (char)('A' + i);
    uint16_t col = (i == studio_pattern()) ? C_GOLD
                   : (i < studio_pattern_limit() ? C_DIM : C_DIMMER);
    char one[2] = { c, 0 };
    text(d, 34 + i * 10, 51, one, col);
  }
  snprintf(b, sizeof(b), "STEP %02d/16", s_seq_step + 1);
  text(d, 214, 51, b, C_DIM);

  /* ruler */
  for (int s = 0; s < 16; s++) {
    int x = 56 + s * 16;
    uint16_t c = (s % 4 == 0) ? C_LINE : C_PANEL;
    d->fillRect(x, 62, 14, 3, c);
    if (s == cur) d->fillRect(x, 61, 14, 5, C_CYAN);
  }

  for (int t = 0; t < 10; t++) {
    int y = 70 + t * 13;
    bool sel = (t == s_seq_track);
    if (sel) d->fillRect(0, y - 1, 320, 13, C_PANEL2);
    d->fillRect(0, y - 1, 2, 13, trk_col(t));
    text(d, 5, y, st_track_name[t], sel ? C_WHITE : C_DIM);
    for (int s = 0; s < 16; s++) {
      int x = 56 + s * 16;
      uint16_t c = C_BG2;
      uint8_t note = song->step[pat][t][s].note;
      if (s >= len) {
        c = C_BG;
      } else if (note != 0xFF) {
        c = trk_col(t);
        if (song->step[pat][t][s].vel < 6) c = C_DIMMER;
      }
      d->fillRect(x, y, 14, 10, c);
      if (s == s_seq_step) d->drawRect(x, y, 14, 10, C_WHITE);
    }
  }

  /* footer */
  int t = s_seq_track, s = s_seq_step;
  snprintf(b, sizeof(b), "%s  STEP %02d/%02d  VEL %d  GATE %d  PEN %d",
           st_track_name[t], s + 1, len,
           song->step[pat][t][s].vel, song->step[pat][t][s].gate,
           s_seq_pencil);
  text(d, 6, 200, b, C_DIM);
  draw_hint(d, "< > ^ v move   Enter edit   Space play  1-5 page");
}

static void draw_syn(lgfx::LovyanGFX *d) {
  d->fillRect(0, 48, 320, 159, C_BG);
  set_font(d, 1);
  char b[64];
  snprintf(b, sizeof(b), "PATCH %d/%d %s TRK %s OCT %+d PEN %d", s_syn_patch + 1,
           ST_PATCHES, st_patch_name[s_syn_patch],
           s_seq_track < 6 ? st_track_name[s_seq_track] : "LIVE", s_syn_octave,
           s_seq_pencil);
  text(d, 6, 51, b, C_DIM);

  for (int i = 0; i < ST_PATCHES; i++) {
    int col = i & 3, row = i >> 2;
    int x = 6 + col * 78, y = 62 + row * 34;
    bool sel = (i == s_syn_patch);
    d->fillRect(x, y, 74, 30, sel ? C_PANEL2 : C_PANEL);
    d->drawRect(x, y, 74, 30, sel ? C_GOLD : C_LINE);
    set_font(d, 1);
    text_bg(d, x + 5, y + 5, st_patch_name[i], sel ? C_WHITE : C_TEXT,
            sel ? C_PANEL2 : C_PANEL);
    snprintf(b, sizeof(b), "ALG %d", st_patches[i].alg);
    text_bg(d, x + 5, y + 17, b, C_DIMMER, sel ? C_PANEL2 : C_PANEL);
  }

  /* 24-key strip */
  for (int k = 0; k < 24; k++) {
    int x = 6 + k * 13;
    int semi = k % 12;
    bool black = (semi == 1 || semi == 3 || semi == 6 || semi == 8 || semi == 10);
    int y = black ? 132 : 136;
    int h = black ? 25 : 42;
    d->fillRect(x, y, black ? 9 : 12, h, black ? C_BG2 : C_WHITE);
    d->drawRect(x, y, black ? 9 : 12, h, k == s_syn_key ? C_GOLD : C_LINE);
    if (!black && (k % 12 == 0)) {
      text_bg(d, x + 2, y + 28, "C", C_DIMMER, C_WHITE);
    }
  }
  d->fillRect(6, 180, 308, 1, C_LINE);
  text(d, 6, 186, "OCT", C_DIM);
  for (int o = -2; o <= 2; o++) {
    int x = 40 + (o + 2) * 50;
    bool sel = (o == s_syn_octave);
    if (sel) d->fillRect(x, 184, 44, 14, C_PANEL2);
    d->drawRect(x, 184, 44, 14, sel ? C_GOLD : C_LINE);
    snprintf(b, sizeof(b), "%+d", o);
    text_bg(d, x + 14, 188, b, sel ? C_WHITE : C_DIM,
            sel ? C_PANEL2 : C_BG);
  }
  draw_hint(d, "^ v oct  Tab+^ v patch  [ ] ARP  keys=piano");
}

static void draw_mix(lgfx::LovyanGFX *d) {
  d->fillRect(0, 48, 320, 159, C_BG);
  struct st_song *song = studio_song();
  set_font(d, 1);
  for (int t = 0; t < 10; t++) {
    int y = 52 + t * 14;
    bool sel = (t == s_mix_sel);
    if (sel) d->fillRect(0, y - 1, 280, 14, C_PANEL2);
    d->fillRect(0, y - 1, 2, 14, trk_col(t));
    text(d, 6, y, st_track_name[t], sel ? C_WHITE : C_DIM);
    int lv = song->level[t] & 15;
    d->fillRect(44, y + 3, 120, 6, C_PANEL);
    d->fillRect(44, y + 3, (lv * 120) / 15, 6,
                song->mute[t] ? C_RED : trk_col(t));
    text(d, 172, y, song->mute[t] ? "MUTE" : "ON",
         song->mute[t] ? C_RED : C_GREEN);
    d->fillRect(220, y + 3, 52, 6, C_PANEL);
    d->fillRect(220, y + 3, (lv * 52) / 15, 6, trk_col(t));
  }
  /* master */
  int y = 52 + 10 * 14;
  bool msel = (s_mix_sel == 10);
  if (msel) d->fillRect(0, y - 1, 280, 14, C_PANEL2);
  text(d, 6, y, "MASTER", msel ? C_WHITE : C_GOLD);
  d->fillRect(60, y + 3, 150, 6, C_PANEL);
  d->fillRect(60, y + 3, (g_volume * 150) / 10, 6, C_GOLD);
  char mb[16];
  snprintf(mb, sizeof(mb), "%d/10", g_volume);
  text(d, 220, y, mb, msel ? C_GOLD : C_CYAN);

  /* master VU */
  for (int c = 0; c < 2; c++) {
    int x = 288 + c * 12;
    d->fillRect(x, 52, 8, 122, C_PANEL);
    int lv = studio_level(c);
    int h = (lv * 118) / 100;
    d->fillRect(x, 170 - h, 8, h, c ? C_CYAN : C_GREEN);
  }
  draw_hint(d, "< > ^ v select track   Space play   MIX levels");
}

static void draw_vis(lgfx::LovyanGFX *d) {
  d->fillRect(0, 48, 320, 159, C_BG);
  if (s_vis_scope) {
    const int16_t *sc = studio_scope();
    d->drawFastHLine(8, 126, 304, C_LINE);
    for (int i = 0; i < 128; i++) {
      int x = 8 + i * 2;
      int y = 126 - (sc[i] * 54) / 32768;
      if (y < 64) y = 64;
      if (y > 188) y = 188;
      d->drawFastVLine(x, y, 1, C_CYAN);
    }
    text(d, 8, 54, "TRIGGERED SCOPE / 128 SAMPLES", C_DIM);
  } else {
    const uint8_t *sp = studio_spectrum();
    const uint8_t *pk = studio_spectrum_peak();
    for (int i = 0; i < 64; i++) {
      int x = 8 + i * 4;
      int h = sp[i];
      int y = 190 - h;
      uint16_t c = h > 52 ? C_RED : (h > 30 ? C_YELLOW : C_GREEN);
      d->fillRect(x, y, 3, h, c);
      int py = 190 - pk[i];
      if (pk[i]) d->drawFastHLine(x, py, 3, C_WHITE);
    }
    text(d, 8, 54, "64-BIN LOG FFT / PEAK HOLD", C_DIM);
  }
  char b[64];
  snprintf(b, sizeof(b), "CPU %d%%  UND %d  FPS %d  FM 24/SSG 6/DRM 3",
           g_cpu_pct, g_underruns, g_fps);
  text(d, 8, 196, b, C_DIMMER);
  draw_hint(d, "Enter FFT/scope   Space play");
}

static void draw_sys(lgfx::LovyanGFX *d) {
  d->fillRect(0, 48, 320, 159, C_BG);
  static const char *const lab[SYS_ROWS] = {
    "BPM", "Swing", "Chain", "Pattern", "Pages", "Length", "Copy", "Clear",
    "Output", "Volume", "HP Gain", "Backlight",
    "Save", "Load", "Export MID", "Demo", "Song",
    "Battery", "Tilt", "Engine", "Shutdown",
  };
  char val[SYS_ROWS][24];
  memset(val, 0, sizeof(val));
  snprintf(val[SYS_BPM], sizeof(val[0]), "%d", studio_bpm());
  snprintf(val[SYS_SWING], sizeof(val[0]), "%d%%", studio_swing());
  snprintf(val[SYS_CHAIN], sizeof(val[0]), "%s", studio_chain() ? "ON" : "OFF");
  snprintf(val[SYS_PATTERN], sizeof(val[0]), "%c", 'A' + studio_pattern());
  snprintf(val[SYS_PAGES], sizeof(val[0]), "%d", studio_pattern_limit());
  snprintf(val[SYS_LENGTH], sizeof(val[0]), "%d/16", studio_pattern_length(studio_pattern()));
  snprintf(val[SYS_COPY], sizeof(val[0]), "Enter");
  snprintf(val[SYS_CLEAR], sizeof(val[0]), "Enter");
  snprintf(val[SYS_OUTPUT], sizeof(val[0]), "%s", g_headphones ? "HEADPHONES" : "Speaker");
  snprintf(val[SYS_VOLUME], sizeof(val[0]), g_headphones ? "%d/10 HP" : "%d/10", g_volume);
  snprintf(val[SYS_HP_GAIN], sizeof(val[0]), "%d%%", g_hp_att);
  snprintf(val[SYS_BACKLIGHT], sizeof(val[0]), "%d", g_backlight);
  snprintf(val[SYS_SAVE], sizeof(val[0]), "Enter");
  snprintf(val[SYS_LOAD], sizeof(val[0]), "Enter");
  snprintf(val[SYS_EXPORT], sizeof(val[0]), "MID");
  snprintf(val[SYS_DEMO], sizeof(val[0]), "Enter");
  snprintf(val[SYS_SONG], sizeof(val[0]), "FRACTURE");
  int bl = (int)M5.Power.getBatteryLevel();
  snprintf(val[SYS_BATTERY], sizeof(val[0]), bl < 0 ? "--" : "%d%%", bl);
  snprintf(val[SYS_TILT], sizeof(val[0]), "X%d Y%d", g_tilt_x / 10, g_tilt_y / 10);
  snprintf(val[SYS_ENGINE], sizeof(val[0]), "OPNA 6FM/3SSG");
  snprintf(val[SYS_SHUTDOWN], sizeof(val[0]), "Enter");

  int top = s_sys_sel - 4;
  if (top < 0) top = 0;
  if (top > SYS_ROWS - 10) top = SYS_ROWS - 10;
  s_sys_top = top;

  set_font(d, 1);
  for (int i = 0; i < 10; i++) {
    int row = top + i;
    int y = 50 + i * 14;
    bool sel = (row == s_sys_sel);
    if (sel) d->fillRect(0, y - 1, 320, 14, C_PANEL2);
    d->fillRect(0, y - 1, 2, 14, C_PURPLE);
    text(d, 8, y, lab[row], sel ? C_WHITE : C_TEXT);
    text_r(d, 312, y, val[row], sel ? C_GOLD : C_CYAN);
  }
  if (s_msg[0]) draw_hint(d, s_msg);
  else draw_hint(d, "^ v row   < > value   Enter execute");
}

static void draw_external() {
  if (!s_ext_cv) return;
  lgfx::LovyanGFX *d = s_ext_cv;
  d->fillScreen(C_BG);
  draw_status(d);
  draw_header(d);
  switch (s_page) {
  case 0: draw_seq(d); break;
  case 1: draw_syn(d); break;
  case 2: draw_mix(d); break;
  case 3: draw_vis(d); break;
  default: draw_sys(d); break;
  }
  draw_tabs(d);
  s_ext_cv->pushSprite(0, 0);
}

static void draw_hud() {
  if (!s_hud_cv) return;
  lgfx::LovyanGFX *d = s_hud_cv;
  d->fillScreen(C_BG);
  d->fillRect(0, 0, 240, 16, C_PANEL);
  set_font(d, 1);
  text_bg(d, 4, 4, k_page[s_page], s_page_accent[s_page], C_PANEL);
  text_bg(d, 44, 4, studio_playing() ? "PLAY" : "STOP",
          studio_playing() ? C_GREEN : C_DIMMER, C_PANEL);
  text_bg(d, 202, 4, s_sd_ok ? "SD" : "NO", s_sd_ok ? C_GREEN : C_RED, C_PANEL);
  d->fillRect(0, 15, 240, 1, s_page_accent[s_page]);

  char b[48];
  struct st_song *song = studio_song();
  int pat = studio_pattern();
  switch (s_page) {
  case 0: {
    for (int i = 0; i < 10; i++) {
      int y = 22 + i * 8;
      int lv = song->level[i] & 15;
      d->fillRect(4, y, 132, 6, C_PANEL);
      d->fillRect(4, y, (lv * 132) / 15, 6, trk_col(i));
      if (i == s_seq_track) d->drawRect(4, y, 132, 6, C_WHITE);
      set_font(d, 1);
      text(d, 140, y - 1, st_track_name[i], i == s_seq_track ? C_WHITE : C_DIM);
    }
    snprintf(b, sizeof(b), "STEP %02d/%d  PAT %c", s_seq_step + 1, 16, 'A' + pat);
    text(d, 4, 104, b, C_CYAN);
    break;
  }
  case 1: {
    set_font(d, 2);
    text(d, 6, 26, st_patch_name[s_syn_patch], C_GOLD);
    set_font(d, 1);
    snprintf(b, sizeof(b), "KEY %d  OCT %+d  ARP %s  LATCH %s", s_syn_key + 1,
             s_syn_octave, studio_live_arp_get() ? "ON" : "OFF",
             studio_live_latch_get() ? "ON" : "OFF");
    text(d, 6, 52, b, C_CYAN);
    d->drawFastHLine(6, 68, 228, C_LINE);
    text(d, 6, 76, "FM6 live voice / tilt bend", C_DIM);
    break;
  }
  case 2: {
    for (int c = 0; c < 2; c++) {
      int x = 18 + c * 44;
      d->fillRect(x, 22, 30, 80, C_PANEL);
      int lv = studio_level(c);
      int h = (lv * 76) / 100;
      d->fillRect(x, 100 - h, 30, h, c ? C_CYAN : C_GREEN);
      text(d, x + 8, 104, c ? "R" : "L", C_DIM);
    }
    char mb[16]; snprintf(mb, sizeof(mb), "MASTER %d/10", g_volume);
    text(d, 110, 30, mb, C_DIM);
    text(d, 110, 46, "10 tracks + drums", C_DIMMER);
    break;
  }
  case 3: {
    if (!s_vis_scope) {
      const int16_t *sc = studio_scope();
      for (int i = 0; i < 96; i++) {
        int x = 4 + i * 2;
        int y = 64 - (sc[i] * 36) / 32768;
        if (y < 20) y = 20;
        if (y > 100) y = 100;
        d->drawFastVLine(x, y, 1, C_CYAN);
      }
      text(d, 4, 22, "HUD SCOPE", C_DIM);
    } else {
      const uint8_t *sp = studio_spectrum();
      for (int i = 0; i < 48; i++) {
        int x = 4 + i * 4;
        int h = sp[i];
        d->fillRect(x, 100 - h, 3, h, C_GREEN);
      }
      text(d, 4, 22, "HUD FFT", C_DIM);
    }
    break;
  }
  default: {
    snprintf(b, sizeof(b), "SD %s   CHAIN %s", s_sd_ok ? "OK" : "N/A",
             studio_chain() ? "ON" : "OFF");
    text(d, 6, 26, b, C_CYAN);
    text(d, 6, 42, "OPNA 6FM / 3SSG / 6DRM", C_DIM);
    text(d, 6, 58, "< > ^ v  Enter  Space", C_DIMMER);
    text(d, 6, 74, "1-5 pages  Tab MOD", C_DIMMER);
    break;
  }
  }
  d->fillRect(0, 112, 240, 23, C_PANEL);
  set_font(d, 1);
  text_bg(d, 4, 118, "< > ^ v move   Enter act   Space play", C_DIMMER, C_PANEL);
  s_hud_cv->pushSprite(0, 0);
}

int ui_is_synth_page(void) { return s_page == 1; }
int ui_mod_active(void) { return s_mod ? 1 : 0; }
int ui_fn_held(void) { return s_fn_held; }

static int piano_semitone(char c) {
  switch (c) {
  case 'z': return 0;  case 's': return 1;  case 'x': return 2;
  case 'd': return 3;  case 'c': return 4;  case 'v': return 5;
  case 'g': return 6;  case 'b': return 7;  case 'h': return 8;
  case 'n': return 9;  case 'j': return 10; case 'm': return 11;
  case 'q': return 12; case '2': return 13; case 'w': return 14;
  case '3': return 15; case 'e': return 16; case 'r': return 17;
  case '5': return 18; case 't': return 19; case '6': return 20;
  case 'y': return 21; case '7': return 22; case 'u': return 23;
  default: return -1;
  }
}

static int clampi(int v, int lo, int hi);

int ui_piano_key(char c, int down) {
  int n = piano_semitone(c);
  if (n < 0 || s_page != 1) return 0;
  if (down) {
    s_syn_key = n;
    int note = clampi(60 + s_syn_octave * 12 + n, 0, 127);
    s_seq_pencil = note;
    studio_lock();
    studio_live_focus(1);
    studio_live_patch(s_syn_patch);
    studio_live_key(note, 12);
    studio_unlock();
    ESP_LOGI("piano", "key %c -> note %d focus=%d", c, note,
             studio_live_focus_get());
    s_force_ext = s_force_hud = true;
  } else {
    studio_lock();
    studio_live_key(-1, 0);
    studio_unlock();
    s_force_hud = true;
  }
  return 1;
}

void ui_init(lgfx::LovyanGFX *external, lgfx::LovyanGFX *internal, bool sd_ok) {
  s_ext = external;
  s_int = internal;
  s_sd_ok = sd_ok;

  C_BG = s_ext->color565(8, 10, 18);
  C_BG2 = s_ext->color565(12, 15, 27);
  C_PANEL = s_ext->color565(20, 26, 46);
  C_PANEL2 = s_ext->color565(30, 38, 66);
  C_LINE = s_ext->color565(42, 52, 86);
  C_TEXT = s_ext->color565(214, 222, 243);
  C_DIM = s_ext->color565(126, 138, 170);
  C_DIMMER = s_ext->color565(68, 78, 106);
  C_GOLD = s_ext->color565(255, 212, 94);
  C_CYAN = s_ext->color565(99, 217, 255);
  C_GREEN = s_ext->color565(99, 242, 166);
  C_RED = s_ext->color565(255, 92, 110);
  C_ORANGE = s_ext->color565(255, 155, 69);
  C_PURPLE = s_ext->color565(184, 140, 255);
  C_WHITE = s_ext->color565(255, 255, 255);
  C_YELLOW = s_ext->color565(255, 232, 94);
  s_trk[0] = C_GOLD; s_trk[1] = C_ORANGE; s_trk[2] = C_RED;
  s_trk[3] = C_PURPLE; s_trk[4] = C_CYAN; s_trk[5] = C_GREEN;
  s_trk[6] = s_ext->color565(102, 224, 255);
  s_trk[7] = s_ext->color565(79, 214, 200);
  s_trk[8] = s_ext->color565(182, 255, 107);
  s_trk[9] = s_ext->color565(255, 107, 74);
  s_page_accent[0] = C_GOLD; s_page_accent[1] = C_GREEN;
  s_page_accent[2] = C_ORANGE; s_page_accent[3] = C_CYAN;
  s_page_accent[4] = C_PURPLE;

  s_ext_cv = new M5Canvas(s_ext);
  s_ext_cv->setPsram(false);
  s_ext_cv->setColorDepth(16);
  if (!s_ext_cv->createSprite(320, 240)) {
    ESP_LOGE("ui", "external canvas allocation failed");
    return;
  }
  s_hud_cv = new M5Canvas(s_int);
  s_hud_cv->setPsram(false);
  s_hud_cv->setColorDepth(16);
  if (!s_hud_cv->createSprite(240, 135)) {
    ESP_LOGE("ui", "HUD canvas allocation failed");
    return;
  }

  studio_lock();
  studio_set_analysis(0);
  studio_unlock();
  s_force_ext = s_force_hud = true;
  s_last_ext = s_last_hud = 0;
}

static void set_msg(const char *m) {
  strncpy(s_msg, m, sizeof(s_msg) - 1);
  s_msg[sizeof(s_msg) - 1] = 0;
}

static int clampi(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static void select_pattern(int n) {
  int lim = studio_pattern_limit();
  if (n < 0) n = 0;
  if (n >= lim) {
    set_msg("PAGE LIMIT");
    s_force_ext = s_force_hud = true;
    return;
  }
  studio_lock();
  studio_select_pattern(n);
  studio_unlock();
  char b[16];
  snprintf(b, sizeof(b), "PATTERN %c", 'A' + studio_pattern());
  set_msg(b);
  s_force_ext = s_force_hud = true;
}

void ui_key(int key) {
  if (key >= UI_NUM1 && key <= UI_NUM9) {
    int n = key - UI_NUM1;
    if (s_fn_held) { select_pattern(n); return; }
    if (n < 5) { set_page(n); return; }
    return;
  }
  if (key == UI_NUM0) {
    if (s_fn_held) select_pattern(9);
    return;
  }
  switch (key) {
  case UI_PAGE0: set_page(0); break;
  case UI_PAGE1: set_page(1); break;
  case UI_PAGE2: set_page(2); break;
  case UI_PAGE3: set_page(3); break;
  case UI_PAGE4: set_page(4); break;
  case UI_PLAY:
    studio_lock();
    studio_play(!studio_playing());
    studio_unlock();
    set_msg(studio_playing() ? "PLAY" : "STOP");
    s_force_ext = s_force_hud = true;
    break;
  case UI_FN_DOWN:
    s_fn_held = 1;
    s_mod = !s_mod;
    s_force_ext = s_force_hud = true;
    break;
  case UI_FN_UP:
    s_fn_held = 0;
    break;
  case UI_PAGE_PREV:
    set_page((s_page + 4) % 5);
    break;
  case UI_PAGE_NEXT:
    set_page((s_page + 1) % 5);
    break;
  case UI_MOD: s_mod = !s_mod; s_force_ext = s_force_hud = true; break;
  case UI_PAT_PREV:
  case UI_PAT_NEXT: {
    if (s_page == 1) {
      studio_lock();
      if (key == UI_PAT_PREV) studio_live_arp(!studio_live_arp_get());
      else studio_live_latch(!studio_live_latch_get());
      studio_unlock();
      set_msg(key == UI_PAT_PREV ? "ARP" : "LATCH");
      s_force_hud = true;
      break;
    }
    int lim = studio_pattern_limit();
    int n = studio_pattern() + (key == UI_PAT_NEXT ? 1 : -1);
    if (n < 0) n = lim - 1;
    if (n >= lim) n = 0;
    select_pattern(n);
    break;
  }
  case UI_BPM_DOWN:
  case UI_BPM_UP:
    studio_lock();
    studio_set_bpm(clampi(studio_bpm() + (key == UI_BPM_UP ? 1 : -1), 60, 200));
    studio_unlock();
    s_force_ext = s_force_hud = true;
    break;
  case UI_UP:
  case UI_DOWN: {
    int d = (key == UI_DOWN) ? 1 : -1;
    if (s_page == 0) {
      if (s_mod) {
        struct st_song *song = studio_song();
        struct st_step *st = &song->step[studio_pattern()][s_seq_track][s_seq_step];
        studio_lock();
        st->vel = (uint8_t)clampi((int)st->vel - d, 1, 15);
        if (st->note != 0xFF) studio_preview_ex(s_seq_track, st->note, st->vel, st->gate);
        studio_unlock();
        set_msg("VEL");
      } else {
        s_seq_track = (s_seq_track + (d > 0 ? 1 : 9)) % 10;
      }
    } else if (s_page == 1) {
      if (s_mod) {
        s_syn_patch = (s_syn_patch + (d > 0 ? 1 : 7)) % 8;
        studio_lock();
        if (s_seq_track < 6) {
          studio_song()->patch[s_seq_track] = (uint8_t)s_syn_patch;
          studio_sync_song();
        }
        studio_live_patch(s_syn_patch);
        studio_unlock();
        set_msg(st_patch_name[s_syn_patch]);
      } else {
        s_syn_octave = clampi(s_syn_octave - d, -2, 2);
        s_seq_pencil = clampi(60 + s_syn_octave * 12 + s_syn_key, 0, 127);
        char ob[16];
        snprintf(ob, sizeof(ob), "OCT %+d", s_syn_octave);
        set_msg(ob);
      }
    } else if (s_page == 2) {
      s_mix_sel = (s_mix_sel + (d > 0 ? 1 : 10)) % 11;
    } else if (s_page == 4) {
      s_sys_sel = (s_sys_sel + (d > 0 ? 1 : SYS_ROWS - 1)) % SYS_ROWS;
    }
    s_force_ext = s_force_hud = true;
    break;
  }
  case UI_LEFT:
  case UI_RIGHT: {
    int d = (key == UI_RIGHT) ? 1 : -1;
    if (s_page == 0) {
      if (s_mod) {
        struct st_song *song = studio_song();
        struct st_step *st = &song->step[studio_pattern()][s_seq_track][s_seq_step];
        studio_lock();
        st->gate = (uint8_t)clampi((int)st->gate + d, 1, 15);
        if (st->note != 0xFF) studio_preview_ex(s_seq_track, st->note, st->vel, st->gate);
        studio_unlock();
        set_msg("GATE");
      } else {
        s_seq_step = (s_seq_step + (d > 0 ? 1 : 15)) % 16;
      }
    } else if (s_page == 1) {
      s_syn_key = (s_syn_key + (d > 0 ? 1 : 23)) % 24;
      s_seq_pencil = clampi(60 + s_syn_octave * 12 + s_syn_key, 0, 127);
    } else if (s_page == 2) {
      if (s_mix_sel < 10) {
        struct st_song *song = studio_song();
        studio_lock();
        song->level[s_mix_sel] = (uint8_t)clampi(song->level[s_mix_sel] + d, 0, 15);
        studio_sync_song();
        studio_unlock();
      } else {
        g_volume = clampi(g_volume + d, 0, 10);
      }
    } else if (s_page == 4) {
      switch (s_sys_sel) {
      case SYS_OUTPUT:
        g_hp_override ^= 1;
        g_headphones = g_hp_override;
        ESP_LOGI("ui", "output -> %s", g_headphones ? "HEADPHONES" : "SPEAKER");
        break;
      case SYS_VOLUME:
        g_volume = clampi(g_volume + d, 0, 10);
        break;
      case SYS_HP_GAIN: {
        static const int atts[] = {50, 25, 12, 6, 3, 1};
        int cur = g_hp_att, idx = 4;
        for (int i = 0; i < 6; i++) if (atts[i] == cur) idx = i;
        idx = (idx + (d > 0 ? 1 : 5)) % 6;
        g_hp_att = atts[idx];
        break;
      }
      case SYS_BACKLIGHT:
        g_backlight = clampi(g_backlight + d * 16, 8, 255);
        M5.Display.setBrightness((uint8_t)g_backlight);
        break;
      case SYS_BPM:
        studio_lock();
        studio_set_bpm(clampi(studio_bpm() + d, 60, 200));
        studio_unlock();
        break;
      case SYS_SWING:
        studio_lock();
        studio_set_swing(clampi(studio_swing() + d, 0, 50));
        studio_unlock();
        break;
      case SYS_PATTERN: {
        int lim = studio_pattern_limit();
        int n = studio_pattern() + d;
        if (n < 0) n = lim - 1;
        if (n >= lim) n = 0;
        select_pattern(n);
        break;
      }
      case SYS_PAGES: {
        int lim = studio_pattern_limit() + d;
        if (lim < 1) lim = 1;
        if (lim > ST_PATTERNS) lim = ST_PATTERNS;
        studio_lock();
        studio_set_pattern_limit(lim);
        studio_unlock();
        break;
      }
      case SYS_LENGTH:
        studio_lock();
        studio_set_pattern_length(studio_pattern(),
                                  clampi(studio_pattern_length(studio_pattern()) + d,
                                         1, ST_STEPS));
        studio_unlock();
        break;
      default: break;
      }
    }
    s_force_ext = s_force_hud = true;
    break;
  }
  case UI_OK_UP:
    if (s_page == 1) {
      studio_lock();
      studio_live_key(-1, 0);
      studio_unlock();
      s_force_hud = true;
    }
    break;
  case UI_OK:
    if (s_page == 0) {
      struct st_song *song = studio_song();
      int pat = studio_pattern(), t = s_seq_track, stp = s_seq_step;
      struct st_step *st = &song->step[pat][t][stp];
      studio_lock();
      if (t == 9) {
        if (st->note == 0xFF) st->note = 0;
        else if (st->note < 5) st->note++;
        else st->note = 0xFF;
        st->vel = 12;
        st->gate = 15;
      } else {
        if (st->note == 0xFF) {
          st->note = (uint8_t)s_seq_pencil;
          st->vel = 12;
          st->gate = 8;
        } else {
          st->note = 0xFF;
        }
      }
      studio_sync_song();
      if (st->note != 0xFF) studio_preview_ex(t, st->note, st->vel, st->gate);
      studio_unlock();
      set_msg(st->note == 0xFF ? "REST" : "NOTE");
      s_force_ext = s_force_hud = true;
    } else if (s_page == 1) {
      int note = clampi(60 + s_syn_octave * 12 + s_syn_key, 0, 127);
      s_seq_pencil = note;
      studio_lock();
      studio_live_key(note, 12);
      studio_unlock();
      set_msg("LIVE HOLD");
      s_force_hud = true;
    } else if (s_page == 2 && s_mix_sel < 10) {
      struct st_song *song = studio_song();
      studio_lock();
      song->mute[s_mix_sel] ^= 1;
      studio_sync_song();
      studio_unlock();
      s_force_ext = s_force_hud = true;
    } else if (s_page == 3) {
      s_vis_scope = !s_vis_scope;
      s_force_ext = s_force_hud = true;
    } else if (s_page == 4) {
      int pat = studio_pattern();
      switch (s_sys_sel) {
      case SYS_CHAIN:
        studio_lock();
        studio_set_chain(!studio_chain());
        studio_unlock();
        s_force_ext = s_force_hud = true;
        break;
      case SYS_COPY: {
        studio_lock();
        int to = (pat + 1) % studio_pattern_limit();
        studio_copy_pattern(pat, to);
        studio_unlock();
        set_msg("COPIED TO NEXT");
        s_force_ext = s_force_hud = true;
        break;
      }
      case SYS_CLEAR:
        studio_lock();
        studio_clear_pattern(pat);
        studio_unlock();
        set_msg("PATTERN CLEARED");
        s_force_ext = s_force_hud = true;
        break;
      case SYS_SAVE: {
        ESP_LOGI("ui", "SAVE requested");
        studio_lock();
        int ok = persist_save(g_volume, g_backlight);
        studio_unlock();
        set_msg(ok > 0 ? "SAVED TO SD" : "SAVE ERROR");
        s_force_ext = s_force_hud = true;
        break;
      }
      case SYS_LOAD: {
        ESP_LOGI("ui", "LOAD requested");
        studio_lock();
        int v = g_volume, b = g_backlight;
        int ok = persist_load(&v, &b);
        studio_unlock();
        if (ok > 0) {
          g_volume = v;
          g_backlight = b;
          M5.Display.setBrightness((uint8_t)g_backlight);
          set_msg("LOADED");
        } else {
          set_msg("LOAD ERROR");
        }
        s_force_ext = s_force_hud = true;
        break;
      }
      case SYS_EXPORT: {
        studio_lock();
        int ok = midi_export("/sdcard/opna2.mid");
        studio_unlock();
        set_msg(ok > 0 ? "EXPORTED MIDI" : "MIDI ERROR");
        s_force_ext = s_force_hud = true;
        break;
      }
      case SYS_DEMO:
        studio_lock();
        studio_demo_song();
        studio_sync_song();
        studio_unlock();
        set_msg("DEMO");
        s_force_ext = s_force_hud = true;
        break;
      case SYS_SONG:
        studio_lock();
        studio_song_fracture();
        studio_unlock();
        set_msg("FRACTURE PROTOCOL");
        s_force_ext = s_force_hud = true;
        break;
      case SYS_SHUTDOWN:
        set_msg("SHUTDOWN");
        s_force_ext = s_force_hud = true;
        draw_external();
        M5.Power.powerOff();
        break;
      default: break;
      }
    }
    break;
  default: break;
  }
}

void ui_tick() {
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
  static uint32_t last_tilt = 0;
  if ((s_page == 1 || s_page == 4) && (now - last_tilt) >= 50) {
    float ax = 0, ay = 0, az = 0;
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      g_tilt_x = clampi((int)(ax * 1000.0f), -1000, 1000);
      g_tilt_y = clampi((int)(ay * 1000.0f), -1000, 1000);
      if (s_page == 1) {
        studio_lock();
        studio_live_tilt(g_tilt_x, g_tilt_y);
        studio_unlock();
      }
    }
    last_tilt = now;
  }
  s_fps_n++;
  if (now - s_fps_t >= 1000) {
    g_fps = s_fps_n;
    s_fps_n = 0;
    s_fps_t = now;
  }
  if (s_page == 2) {
    studio_lock();
    studio_vu_update();
    studio_unlock();
  } else if (s_page == 3) {
    studio_lock();
    studio_analysis_update();
    studio_unlock();
  }

  if (s_force_ext || (now - s_last_ext) >= 200) {
    draw_external();
    s_last_ext = now;
    s_force_ext = false;
  }
  if (s_force_hud || (now - s_last_hud) >= 50) {
    draw_hud();
    s_last_hud = now;
    s_force_hud = false;
  }
}
