#include "render/platform_surface.h"
#include "../../platform/backends/glfw_private.h"
const char **platform_vulkan_extensions(uint32_t *count) {
  return glfwGetRequiredInstanceExtensions(count);
}
VkResult platform_vulkan_surface(VkInstance instance,PlatformWindow *window,VkSurfaceKHR *surface) {
  return glfwCreateWindowSurface(instance,platform_glfw_native(window),NULL,surface);
}
