/* PORTING (E14): questo file usa l'interoperabilita' GLFW/Vulkan, quindi
 * chiede lui la dichiarazione -- non glfw_helper.h per tutti. */
#define GLFW_INCLUDE_VULKAN
#include "platform/window.h"
#include "render/vulkan_surface.h"
#include "render/gfx_vulkan.h"
#include "log/log.h"
#include <stdlib.h>

VkSurfaceKHR create_surface(APP *app) {

  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(VK_CTX(app)->instance, app->window, NULL, &surface) !=
      VK_SUCCESS) {
    LOG_ERROR("failed to create window surface");
    exit(1);
  }
  return surface;
}

void destroy_surface(APP *app) {

  vkDestroySurfaceKHR(VK_CTX(app)->instance, VK_CTX(app)->render_context.surface,
                      NULL);
}
