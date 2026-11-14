#include "file_manager.h"

#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <esp_log.h>

static const char *TAG = "files";
static char s_path[192] = "/sdcard";
static struct fm_entry s_entries[FM_MAX_ENTRIES];
static int s_count;

static int cmp_entry(const void *a, const void *b) {
  const struct fm_entry *x = (const struct fm_entry *)a;
  const struct fm_entry *y = (const struct fm_entry *)b;
  if (x->dir != y->dir) return x->dir ? -1 : 1;
  return strcasecmp(x->name, y->name);
}

void fm_init(void) {
  strcpy(s_path, "/sdcard");
  fm_refresh();
}

int fm_refresh(void) {
  s_count = 0;
  DIR *d = opendir(s_path);
  if (!d) {
    ESP_LOGE(TAG, "opendir %s failed", s_path);
    return -1;
  }
  if (strcmp(s_path, "/sdcard") != 0) {
    snprintf(s_entries[s_count].name, FM_NAME_LEN, "..");
    s_entries[s_count].dir = true;
    s_entries[s_count].size = 0;
    s_count++;
  }
  struct dirent *e;
  while ((e = readdir(d)) != nullptr && s_count < FM_MAX_ENTRIES) {
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    char full[256];
    size_t pl = strlen(s_path), nl = strlen(e->d_name);
    if (pl + 1 + nl >= sizeof(full)) continue;
    memcpy(full, s_path, pl);
    full[pl] = '/';
    memcpy(full + pl + 1, e->d_name, nl + 1);
    struct stat st;
    if (stat(full, &st) != 0) continue;
    struct fm_entry *x = &s_entries[s_count++];
    strncpy(x->name, e->d_name, FM_NAME_LEN - 1);
    x->name[FM_NAME_LEN - 1] = 0;
    x->dir = S_ISDIR(st.st_mode);
    x->size = (uint32_t)st.st_size;
  }
  closedir(d);
  qsort(s_entries, (size_t)s_count, sizeof(s_entries[0]), cmp_entry);
  ESP_LOGI(TAG, "%s: %d entries", s_path, s_count);
  return s_count;
}

const char *fm_path(void) { return s_path; }
int fm_count(void) { return s_count; }
const struct fm_entry *fm_entry_at(int i) {
  return (i >= 0 && i < s_count) ? &s_entries[i] : nullptr;
}

int fm_enter(int i) {
  const struct fm_entry *e = fm_entry_at(i);
  if (!e || !e->dir) return -1;
  if (!strcmp(e->name, "..")) return fm_parent();
  size_t pl = strlen(s_path), nl = strlen(e->name);
  if (pl + 1 + nl >= sizeof(s_path)) return -1;
  char next[192];
  memcpy(next, s_path, pl);
  next[pl] = '/';
  memcpy(next + pl + 1, e->name, nl + 1);
  strcpy(s_path, next);
  return fm_refresh() >= 0 ? 0 : -1;
}

int fm_parent(void) {
  if (!strcmp(s_path, "/sdcard")) return -1;
  char *p = strrchr(s_path, '/');
  if (!p || p <= s_path + 7) strcpy(s_path, "/sdcard");
  else *p = 0;
  return fm_refresh() >= 0 ? 0 : -1;
}

static const char *ext(const char *name) {
  const char *p = strrchr(name, '.');
  return p ? p : "";
}

int fm_is_m(int i) {
  const struct fm_entry *e = fm_entry_at(i);
  return e && !e->dir && !strcasecmp(ext(e->name), ".m");
}
int fm_is_p2s(int i) {
  const struct fm_entry *e = fm_entry_at(i);
  return e && !e->dir && !strcasecmp(ext(e->name), ".p2s");
}

const char *fm_full_path(int i, char *buf, int cap) {
  const struct fm_entry *e = fm_entry_at(i);
  if (!e) return nullptr;
  snprintf(buf, cap, "%s/%s", s_path, e->name);
  return buf;
}
