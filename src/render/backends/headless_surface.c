#include "render/platform_surface.h"
/* Headless platform tests have no presentation surface. Fail explicitly; this
 * backend does not pretend to implement offscreen Vulkan rendering. */
const char **platform_vulkan_extensions(uint32_t *count) { *count=0; return NULL; }
VkResult platform_vulkan_surface(VkInstance instance,PlatformWindow *window,VkSurfaceKHR *surface) {
  *surface=VK_NULL_HANDLE; return VK_ERROR_EXTENSION_NOT_PRESENT;
}
