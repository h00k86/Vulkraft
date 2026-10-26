


#pragma once
#include <vulkan/vulkan.h>
typedef struct APP APP;
#include "log/log.h"
VkSurfaceKHR create_surface(APP * app);

void destroy_surface(APP * app);
