#pragma once
#include <stdbool.h>
#include "render/gfx_vulkan.h"
typedef struct APP APP;
bool create_ubo_camera(APP *app, UniformBuffer *ub);
void destroy_ubo_camera(APP *app, UniformBuffer *ub);
