/**
 * @file keyboard.cpp
 * @brief TCA8418 matrix mapping for Cardputer ADV.
 *
 * Key policy (v0.4):
 *   - Tab: next page, Backspace: previous page;
 *   - ; . , /: arrows on every page;
 *   - Fn tap: toggle MOD; hold Fn + 1..0: pattern select;
 *   - SYN: letters/2,3,5,6,7 play; other input falls through to UI.
 */#include "keyboard.h"
#include "ui.h"

#include <M5Unified.hpp>
#include <esp_log.h>

static const char *TAG = "kbd";
static constexpr uint8_t TCA_ADDR = 0x34;
static bool s_fn = false;

static bool wr(uint8_t reg, uint8_t val) {
  return M5.In_I2C.writeRegister8(TCA_ADDR, reg, val, 400000);
}
static uint8_t rd(uint8_t reg) {
  return M5.In_I2C.readRegister8(TCA_ADDR, reg, 400000);
}

void keyboard_init() {
  bool ok = true;
  ok = wr(0x1D, 0xFF) && ok;
  ok = wr(0x1E, 0xFF) && ok;
  ok = wr(0x1F, 0x03) && ok;
  ok = wr(0x01, 0x81) && ok;  /* AI | key-event interrupt */
  ok = wr(0x02, 0xFF) && ok;
  for (int i = 0; i < 16; i++) {
    if (rd(0x04) == 0) break;
  }
  s_fn = false;
  if (!ok) ESP_LOGW(TAG, "TCA8418 init errors");
  else ESP_LOGI(TAG, "TCA8418 ready");
}

static char key_char(uint8_t code) {
  switch (code) {
  case 1: return '`';
  case 5: return '1'; case 11: return '2'; case 15: return '3';
  case 21: return '4'; case 25: return '5'; case 31: return '6';
  case 35: return '7'; case 41: return '8'; case 45: return '9';
  case 51: return '0';
  case 6: return 'q'; case 12: return 'w'; case 16: return 'e';
  case 22: return 'r'; case 26: return 't'; case 32: return 'y';
  case 36: return 'u'; case 42: return 'i'; case 46: return 'o';
  case 52: return 'p';
  case 13: return 'a'; case 17: return 's'; case 23: return 'd';
  case 27: return 'f'; case 33: return 'g'; case 37: return 'h';
  case 43: return 'j'; case 47: return 'k'; case 53: return 'l';
  case 18: return 'z'; case 24: return 'x'; case 28: return 'c';
  case 34: return 'v'; case 38: return 'b'; case 44: return 'n';
  case 48: return 'm';
  case 54: return ','; case 55: return '-'; case 58: return '.'; case 57: return ';';
  case 64: return '/'; case 56: return '['; case 62: return ']';
  case 61: return '='; case 63: return 0; case 66: return 0;
  default: return 0;
  }
}

static void press(uint8_t code) {
  if (ui_text_active()) {
    if (code == 67) { ui_text_commit(); return; }
    if (code == 65) { ui_text_backspace(); return; }
    if (code == 1) { ui_text_cancel(); return; }
    char tc = key_char(code);
    if (tc) ui_text_char(tc);
    return;
  }
  if (code == 3) { s_fn = true; ui_key(UI_FN_DOWN); return; }
  if (code == 2) { ui_key(UI_PAGE_NEXT); return; }
  if (code == 65) { ui_key(UI_PAGE_PREV); return; }
  if (code == 67) { ui_key(UI_OK); return; }
  if (code == 68) { ui_key(UI_PLAY); return; }

  char c = key_char(code);
  if (!c) return;
  ESP_LOGI("kbd", "char=%c synth=%d mod=%d fn=%d", c, ui_is_synth_page(),
           ui_mod_active(), (int)s_fn);

  if (code == 56) { ui_key(UI_PAT_PREV); return; }
  if (code == 62) { ui_key(UI_PAT_NEXT); return; }
  if (code == 55) { ui_key(UI_BPM_DOWN); return; }
  if (code == 61) { ui_key(UI_BPM_UP); return; }

  if (ui_is_synth_page()) {
    if (c >= '0' && c <= '9') {
      if (!ui_fn_held()) {
        if (ui_piano_key(c, 1)) return;
        return;
      }
    } else if (c >= 'a' && c <= 'z') {
      ui_piano_key(c, 1);
      return;  /* WASD are piano keys, not arrows, on SYN */
    }
  }

  switch (c) {
  case ';': ui_key(UI_UP); return;
  case '.': ui_key(UI_DOWN); return;
  case ',': ui_key(UI_LEFT); return;
  case '/': ui_key(UI_RIGHT); return;
  case '1': ui_key(UI_NUM1); break;
  case '2': ui_key(UI_NUM2); break;
  case '3': ui_key(UI_NUM3); break;
  case '4': ui_key(UI_NUM4); break;
  case '5': ui_key(UI_NUM5); break;
  case '6': ui_key(UI_NUM6); break;
  case '7': ui_key(UI_NUM7); break;
  case '8': ui_key(UI_NUM8); break;
  case '9': ui_key(UI_NUM9); break;
  case '0': ui_key(UI_NUM0); break;
  case '`': ui_key(UI_BACK); break;
  default: break;
  }
}

static void release(uint8_t code) {
  if (code == 3) { s_fn = false; ui_key(UI_FN_UP); }
  if (code == 67) ui_key(UI_OK_UP);
  char c = key_char(code);
  if (c && ui_is_synth_page()) {
    if (!(c >= '0' && c <= '9' && ui_fn_held())) ui_piano_key(c, 0);
  }
}

void keyboard_poll() {
  uint8_t status = rd(0x02);
  if (!(status & 0x01)) return;
  for (int i = 0; i < 10; i++) {
    uint8_t ev = rd(0x04);
    if (ev == 0) break;
    uint8_t code = ev & 0x7F;
    if (ev & 0x80) press(code);
    else release(code);
  }
  wr(0x02, 0x03);
}
