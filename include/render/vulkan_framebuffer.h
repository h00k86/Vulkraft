#pragma once
#include <stdbool.h>
#include "render/gfx_vulkan.h"
typedef struct APP APP;
bool create_framebuffers(APP *app);
void destroy_framebuffers(APP *app);
