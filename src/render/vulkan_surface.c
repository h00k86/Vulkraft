#include "render/platform_surface.h"
#include "platform/window.h"
#include "render/vulkan_surface.h"
#include "render/gfx_vulkan.h"
#include "log/log.h"
#include <stdlib.h>

VkSurfaceKHR create_surface(APP *app) {

  VkSurfaceKHR surface;
  if (platform_vulkan_surface(VK_CTX(app)->instance, app->window, &surface) !=
      VK_SUCCESS) {
    LOG_ERROR("failed to create window surface");
    return VK_NULL_HANDLE;
  }
  return surface;
}

void destroy_surface(APP *app) {

  vkDestroySurfaceKHR(VK_CTX(app)->instance, VK_CTX(app)->render_context.surface,
                      NULL);
}
