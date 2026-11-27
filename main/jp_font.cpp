#include "jp_font.h"
#include "jp_font_data.h"

static int glyph_for(uint16_t code) {
  int lo = 0, hi = JP_FONT_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    if (kJpCode[mid] == code) return mid;
    if (kJpCode[mid] < code) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}

static void draw_glyph(lgfx::LovyanGFX *d, int x, int y, int idx, uint16_t color) {
  if (idx < 0) idx = glyph_for('?');
  if (idx < 0) return;
  for (int row = 0; row < 12; row++) {
    uint16_t bits = ((uint16_t)kJpBitmap[idx][row * 2] << 8) |
                    kJpBitmap[idx][row * 2 + 1];
    for (int col = 0; col < 12; col++) {
      if (bits & (0x8000u >> col)) d->drawPixel(x + col, y + row, color);
    }
  }
}

/* Returns the number of bytes consumed and writes the CP932 code to *code. */
static int next_code(const unsigned char *s, uint16_t *code) {
  unsigned char b = s[0];
  if (b == 0x1b) {                 /* skip a simple ANSI escape */
    int n = 1;
    if (s[1] == '[') {
      n = 2;
      while (s[n] && (s[n] < 0x40 || s[n] > 0x7e)) n++;
      if (s[n]) n++;
    }
    *code = 0;
    return n;
  }
  if (b < 0x20) { *code = 0; return 1; }
  if (b < 0x80) { *code = b; return 1; }
  if (b >= 0xa1 && b <= 0xdf) { *code = b; return 1; }
  if (!s[1]) { *code = '?'; return 1; }
  *code = ((uint16_t)b << 8) | s[1];
  return 2;
}

static int advance_of(uint16_t c) {
  return (c < 0x80 || (c >= 0xa1 && c <= 0xdf)) ? 6 : 12;
}

int jp_has_wide(const char *cp932) {
  if (!cp932) return 0;
  for (const unsigned char *p = (const unsigned char *)cp932; *p; p++)
    if (*p >= 0x80) return 1;
  return 0;
}

void jp_draw_title_right(lgfx::LovyanGFX *d, int right, int y,
                         const char *cp932, uint16_t color) {
  if (!d || !cp932) return;
  const unsigned char *s = (const unsigned char *)cp932;
  uint16_t codes[64];
  int n = 0;
  while (*s && n < 64) {
    uint16_t c;
    int used = next_code(s, &c);
    if (!used) break;
    s += used;
    if (c) codes[n++] = c;
  }
  int total = 0;
  for (int i = 0; i < n; i++) total += advance_of(codes[i]);
  while (n > 0 && total > right - 6) total -= advance_of(codes[--n]);
  int x = right - total;
  for (int i = 0; i < n; i++) {
    draw_glyph(d, x, y, glyph_for(codes[i]), color);
    x += advance_of(codes[i]);
  }
}


static void draw_glyph8(lgfx::LovyanGFX *d, int x, int y, int idx, uint16_t color) {
  if (idx < 0) idx = glyph_for('?');
  if (idx < 0) return;
  for (int oy = 0; oy < 8; oy++) {
    int sy = oy * 12 / 8;
    uint16_t bits = ((uint16_t)kJpBitmap[idx][sy * 2] << 8) |
                    kJpBitmap[idx][sy * 2 + 1];
    for (int ox = 0; ox < 8; ox++) {
      int sx = ox * 12 / 8;
      if (bits & (0x8000u >> sx)) d->drawPixel(x + ox, y + oy, color);
    }
  }
}

/* Header title: full-width glyphs keep the native 12x12 bitmap.  ASCII uses
   Font0 (6x8) and is vertically centred against the 12px kana/kanji. */
void jp_draw_title_header(lgfx::LovyanGFX *d, int right, int y,
                          const char *cp932, uint16_t color) {
  if (!d || !cp932) return;
  const unsigned char *s = (const unsigned char *)cp932;
  uint16_t codes[64];
  int n = 0;
  while (*s && n < 64) {
    uint16_t c;
    int used = next_code(s, &c);
    if (!used) break;
    s += used;
    if (c) codes[n++] = c;
  }
  int total = 0;
  for (int i = 0; i < n; i++) total += (codes[i] < 0x80) ? 6 : 12;
  /* Leave the left page label (SEQ/FILE) untouched. */
  while (n > 0 && total > right - 50) total -= (codes[--n] < 0x80) ? 6 : 12;
  int x = right - total;
  d->setFont(&lgfx::fonts::Font0);
  d->setTextSize(1);
  d->setTextDatum(textdatum_t::top_left);
  d->setTextColor(color);
  for (int i = 0; i < n; i++) {
    if (codes[i] < 0x80) {
      d->drawChar((uint16_t)codes[i], x, y + 2);
      x += 6;
    } else {
      draw_glyph(d, x, y, glyph_for(codes[i]), color);
      x += 12;
    }
  }
}
