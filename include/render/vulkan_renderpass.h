#pragma once
#include <stdbool.h>
#include "render/gfx_vulkan.h"
typedef struct APP APP;
bool create_renderpass(APP *app);
void destroy_renderpass(APP *app);
