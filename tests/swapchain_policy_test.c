#include <stdlib.h>
#include <assert.h>
static int allocations;
static void *tracked_malloc(size_t n) { void *p=malloc(n); if(p) ++allocations; return p; }
static void tracked_free(void *p) { if(p) --allocations; free(p); }
#define malloc tracked_malloc
#define free tracked_free
#include "../src/render/vulkan_swapchain.c"
#undef malloc
#undef free
float g_virtualWidth, g_virtualHeight;
static int image_query_failure, swapchains;
bool vsync_get_enabled(void) { return true; }
void platform_framebuffer_size(PlatformWindow *w,int *x,int *y) { *x=800; *y=600; }
void platform_window_size(PlatformWindow *w,int *x,int *y) { *x=800; *y=600; }
VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice p,VkSurfaceKHR s,VkSurfaceCapabilitiesKHR *c) {
  *c=(VkSurfaceCapabilitiesKHR){.minImageCount=2,.maxImageCount=3,.currentExtent={800,600},
    .supportedUsageFlags=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .supportedTransforms=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .currentTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .supportedCompositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
  return VK_SUCCESS;
}
VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice p,VkSurfaceKHR s,uint32_t *count,VkSurfaceFormatKHR *formats) {
  *count=1; if(formats) *formats=(VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_SRGB,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  return VK_SUCCESS;
}
VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice p,VkSurfaceKHR s,uint32_t *count,VkPresentModeKHR *modes) {
  *count=1; if(modes) *modes=VK_PRESENT_MODE_FIFO_KHR; return VK_SUCCESS;
}
VkResult vkCreateSwapchainKHR(VkDevice d,const VkSwapchainCreateInfoKHR *i,const VkAllocationCallbacks *a,VkSwapchainKHR *s) {
  *s=(VkSwapchainKHR)(uintptr_t)1; ++swapchains; return VK_SUCCESS;
}
VkResult vkGetSwapchainImagesKHR(VkDevice d,VkSwapchainKHR s,uint32_t *count,VkImage *images) {
  if(image_query_failure == (images?2:1)) return VK_ERROR_OUT_OF_HOST_MEMORY;
  *count=2; if(images) { images[0]=(VkImage)(uintptr_t)1; images[1]=(VkImage)(uintptr_t)2; }
  return VK_SUCCESS;
}
void vkDestroySwapchainKHR(VkDevice d,VkSwapchainKHR s,const VkAllocationCallbacks *a) { --swapchains; }
void vkDestroyImageView(VkDevice d,VkImageView v,const VkAllocationCallbacks *a) { assert(0); }

#include <assert.h>
int main(void) {
  VkSurfaceCapabilitiesKHR c = {.minImageCount=2, .maxImageCount=2,
    .currentExtent={UINT32_MAX,UINT32_MAX}, .minImageExtent={64,64}, .maxImageExtent={1024,768},
    .supportedUsageFlags=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .supportedTransforms=VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
    .currentTransform=VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,
    .supportedCompositeAlpha=VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
  VkSurfaceFormatKHR f = {VK_FORMAT_R8G8B8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  VkSwapchainCreateInfoKHR i = {0};
  assert(choose_surface(&c, &f, 1, 2048, 20, true, &i));
  assert(i.imageFormat == f.format && i.imageColorSpace == f.colorSpace);
  assert(i.imageExtent.width == 1024 && i.imageExtent.height == 64);
  assert(i.minImageCount == 2 && i.compositeAlpha == VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR);
  assert(i.preTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR);
  assert(i.imageUsage == VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
  c.supportedUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  assert(choose_surface(&c,&f,1,800,600,true,&i) && (i.imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
  assert(choose_surface(&c,&f,1,800,600,false,&i) && !(i.imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT));
  f.format = VK_FORMAT_UNDEFINED;
  assert(choose_surface(&c,&f,1,800,600,false,&i) && i.imageFormat == VK_FORMAT_B8G8R8A8_SRGB);
  c.supportedUsageFlags = 0;
  assert(!choose_surface(&c,&f,1,800,600,false,&i));
  VulkanContext context={.logical_device=(VkDevice)(uintptr_t)1};
  APP app={.context=&context,.window=(PlatformWindow *)(uintptr_t)1};
  for(image_query_failure=0;image_query_failure<=2;++image_query_failure) {
    context.render_context.swapchain=create_swapchain(&app);
    assert(!!context.render_context.swapchain.handle == !image_query_failure);
    if(!image_query_failure) assert(allocations==1 && swapchains==1);
    destroy_swapchain(&app); destroy_swapchain(&app);
    assert(!allocations && !swapchains);
  }
  puts("Swapchain capability negotiation, ownership and query failure cleanup: PASS");
}
