#pragma once
#include <stdbool.h>
#include "render/gfx_vulkan.h"
typedef struct APP APP;
VkSwapchainCreateInfoKHR create_swapchain_info(APP *app);
bool create_image_views(APP *app);
Swapchain create_swapchain(APP *app);
void destroy_swapchain(APP *app);
void recreate_swapchain(APP *app);
void vsync_toggle_and_apply(APP *app, bool enabled);
