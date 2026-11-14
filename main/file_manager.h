#pragma once
#include <stdint.h>

#define FM_MAX_ENTRIES 64
#define FM_NAME_LEN 32

struct fm_entry {
  char name[FM_NAME_LEN];
  uint32_t size;
  bool dir;
};

void fm_init(void);
const char *fm_path(void);
int fm_count(void);
const struct fm_entry *fm_entry_at(int i);
int fm_refresh(void);
int fm_enter(int i);       /* enter directory, 0 on success */
int fm_parent(void);       /* go to parent, 0 on success */
int fm_is_m(int i);
int fm_is_p2s(int i);
const char *fm_full_path(int i, char *buf, int cap);
