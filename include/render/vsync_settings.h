#pragma once

#include <stdbool.h>

typedef struct APP APP;

bool vsync_get_enabled(void);
void vsync_set_enabled(bool enabled);
void vsync_toggle_and_apply(APP *app, bool enabled);
