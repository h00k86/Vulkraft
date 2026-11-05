#pragma once
#include <stdbool.h>
#include "render/gfx_vulkan.h"
typedef struct APP APP;
bool create_descriptor_set_layout(APP *app);
bool create_descriptor_pool(APP *app);
bool create_hud_descriptor_set(APP *app);
void destroy_descriptors(APP *app);

void update_descriptor_texture_slot(APP *app, int slot, Texture *texture);
void update_descriptor_texture_slot_no_wait(APP *app, int slot, Texture *texture);
