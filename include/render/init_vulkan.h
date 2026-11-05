#pragma once
#include "render/gfx_vulkan.h"
typedef struct APP APP;
bool init_vulkan(APP *app);
void destroy_vulkan(APP *app);
