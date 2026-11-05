
#pragma once
#include "render/gfx_vulkan.h"
#include <stdlib.h>
typedef struct APP APP;

VkDevice create_logical_device(APP * app);

void destroy_logical_device(APP * app);
