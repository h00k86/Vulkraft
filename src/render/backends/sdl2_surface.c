#include "render/platform_surface.h"
#include "../../platform/backends/sdl2_private.h"
#include <SDL_vulkan.h>

const char **platform_vulkan_extensions(uint32_t *count) {
  if(!count) return NULL;
  *count=0;
  SDL_Window *native=platform_sdl2_active_native(); unsigned required=0;
  if(!native || !SDL_Vulkan_GetInstanceExtensions(native,&required,NULL) || !required) return NULL;
  const char **names=platform_sdl2_extension_storage(required);
  if(!names || !SDL_Vulkan_GetInstanceExtensions(native,&required,names)) return NULL;
  *count=required; return names;
}
VkResult platform_vulkan_surface(VkInstance instance,PlatformWindow *window,VkSurfaceKHR *surface) {
  if(surface) *surface=VK_NULL_HANDLE;
  SDL_Window *native=platform_sdl2_native(window);
  if(instance==VK_NULL_HANDLE || !native || !surface) return VK_ERROR_INITIALIZATION_FAILED;
  if(SDL_Vulkan_CreateSurface(native,instance,surface)) return VK_SUCCESS;
  *surface=VK_NULL_HANDLE; return VK_ERROR_INITIALIZATION_FAILED;
}
