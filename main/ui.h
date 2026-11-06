#pragma once
#include <stdint.h>
#include <M5GFX.h>

enum {
  UI_UP = 1, UI_DOWN, UI_LEFT, UI_RIGHT,
  UI_OK, UI_OK_UP, UI_PLAY, UI_MOD, UI_BACK, UI_TAB_DOWN, UI_TAB_UP,
  UI_FN_DOWN, UI_FN_UP,
  UI_PAGE_PREV, UI_PAGE_NEXT,
  UI_PAT_PREV, UI_PAT_NEXT, UI_BPM_DOWN, UI_BPM_UP,
  UI_NUM1, UI_NUM2, UI_NUM3, UI_NUM4, UI_NUM5,
  UI_NUM6, UI_NUM7, UI_NUM8, UI_NUM9, UI_NUM0,
  UI_PAGE0, UI_PAGE1, UI_PAGE2, UI_PAGE3, UI_PAGE4
};

void ui_init(lgfx::LovyanGFX *external, lgfx::LovyanGFX *internal, bool sd_ok);
void ui_key(int key);
void ui_tick(void);
int ui_is_synth_page(void);
int ui_mod_active(void);
int ui_fn_held(void);
int ui_piano_key(char c, int down);
