#pragma once
#include "render/gfx.h"
#define TEXTURE_PREVIEW_SLOT (MAX_TEXTURE_SLOTS - 1)
int texture_manager_load(APP *app, const char *path);
int texture_manager_load_tiled(APP *app, const char *path);
void texture_manager_ensure_repeat(APP *app, int slot);
bool texture_manager_is_valid(int slot);
int texture_manager_free_slots(void);
int texture_manager_loaded_count(void);
bool texture_manager_check_hot_reload(APP *app, int slot);
void texture_manager_begin_batch(APP *app);
void texture_manager_end_batch(void);
void texture_manager_unload_all(APP *app);
void texture_manager_shutdown(APP *app);
const char *texture_manager_get_path(int slot);
bool texture_manager_get_size(int slot, int *outW, int *outH);
int texture_manager_preview_load(APP *app, const char *path);
void texture_manager_preview_clear(APP *app);
