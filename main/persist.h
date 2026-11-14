#pragma once
int persist_save(int volume, int backlight);
int persist_load(int *volume, int *backlight);
int persist_save_path(const char *path, int volume, int backlight);
int persist_load_path(const char *path, int *volume, int *backlight);
int persist_read_last(char *name, int cap);
int persist_write_last(const char *name);
int persist_load_last(int *volume, int *backlight);
