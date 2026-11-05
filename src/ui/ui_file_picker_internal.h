#pragma once
#include "platform/paths.h"
#define MAX_UI_FILE_PICKER_ENTRIES 256
#define UI_PICKER_FAVORITES_MAX 24
#define UI_PICKER_RECENTS_MAX 10
typedef struct {
  char name[128];
  bool directory;
} UIFilePickerEntry;
typedef struct {
  char clicked_path[160];
  bool open, overwrite_pending;
  char id[UI_ID_CAPACITY], title[96];
  UIFilePickerMode mode;
  char root[APP_PATH_MAX], current[APP_PATH_MAX];
  char extension[24], filename[128], search[96], selected[APP_PATH_MAX], error[128];
  UIFilePickerEntry entries[MAX_UI_FILE_PICKER_ENTRIES];
  bool image_preview;
  int entry_count, refresh_count;
  /* palette preferiti/recenti (2026-09-18), percorsi relativi alla root */
  char favorites_key[32];
  char favorites[UI_PICKER_FAVORITES_MAX][160];
  int favorite_count;
  char recents[UI_PICKER_RECENTS_MAX][160];
  int recent_count;
} UIFilePickerState;
