#include "app/application.h"
#include "render/gfx_vulkan.h"
#include "render/vulkan_error.h"
#include "render/vulkan_swapchain.h"
#include "render/vulkan_sync.h"
#include "render/vsync_settings.h"
#include "render/vulkan_framebuffer.h"
#include "render/vulkan_pipeline.h"
#include "render/vulkan_renderpass.h"
#include "log/log.h"
#include "platform/window.h"
#include <stdlib.h>

static VkPresentModeKHR choose_present_mode(APP *app) {
  if (vsync_get_enabled() && !getenv("APP_NO_VSYNC")) return VK_PRESENT_MODE_FIFO_KHR;
  uint32_t count = 0;
  VkPhysicalDevice physical = VK_CTX(app)->physical_device;
  VkSurfaceKHR surface = VK_CTX(app)->render_context.surface;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, NULL) != VK_SUCCESS || !count)
    return VK_PRESENT_MODE_FIFO_KHR;
  VkPresentModeKHR *modes = malloc((size_t)count * sizeof(*modes));
  if (!modes) return VK_PRESENT_MODE_FIFO_KHR;
  VkPresentModeKHR selected = VK_PRESENT_MODE_FIFO_KHR;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &count, modes) == VK_SUCCESS) {
    for (uint32_t i = 0; i < count; ++i)
      if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) selected = modes[i];
    for (uint32_t i = 0; i < count; ++i)
      if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) selected = modes[i];
  }
  free(modes);
  return selected;
}

/* Pure policy function, also exercised with synthetic surface capabilities. */
static bool choose_surface(const VkSurfaceCapabilitiesKHR *caps,
                            const VkSurfaceFormatKHR *formats, uint32_t count,
                            uint32_t width, uint32_t height, bool capture,
                            VkSwapchainCreateInfoKHR *info) {
  if (!count || !(caps->supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) ||
      !caps->supportedCompositeAlpha || !caps->supportedTransforms) return false;
  VkSurfaceFormatKHR format = formats[0];
  if (count == 1 && format.format == VK_FORMAT_UNDEFINED) format.format = VK_FORMAT_B8G8R8A8_SRGB;
  for (uint32_t i = 0; i < count; ++i)
    if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      format = formats[i];
  for (uint32_t i = 0; i < count; ++i)
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      format = formats[i];
  info->imageFormat = format.format; info->imageColorSpace = format.colorSpace;
  info->minImageCount = caps->minImageCount > 3 ? caps->minImageCount : 3;
  if (caps->maxImageCount && info->minImageCount > caps->maxImageCount) info->minImageCount = caps->maxImageCount;
  info->imageExtent = caps->currentExtent;
  if (info->imageExtent.width == UINT32_MAX) {
    info->imageExtent.width = width < caps->minImageExtent.width ? caps->minImageExtent.width : width;
    info->imageExtent.height = height < caps->minImageExtent.height ? caps->minImageExtent.height : height;
    if (info->imageExtent.width > caps->maxImageExtent.width) info->imageExtent.width = caps->maxImageExtent.width;
    if (info->imageExtent.height > caps->maxImageExtent.height) info->imageExtent.height = caps->maxImageExtent.height;
  }
  if (!info->imageExtent.width || !info->imageExtent.height) return false;
  info->imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  bool capture_format = format.format == VK_FORMAT_B8G8R8A8_SRGB || format.format == VK_FORMAT_R8G8B8A8_SRGB ||
                        format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM;
  if (capture && capture_format && (caps->supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    info->imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  info->preTransform = (caps->supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
                           ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps->currentTransform;
  if (!(info->preTransform & caps->supportedTransforms)) return false;
  const VkCompositeAlphaFlagBitsKHR alpha[] = {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
  for (unsigned i = 0; i < sizeof(alpha) / sizeof(alpha[0]); ++i)
    if (caps->supportedCompositeAlpha & alpha[i]) { info->compositeAlpha = alpha[i]; break; }
  info->imageArrayLayers = 1;
  info->imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info->clipped = capture && (info->imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ? VK_FALSE : VK_TRUE;
  return info->compositeAlpha != 0;
}

VkSwapchainCreateInfoKHR create_swapchain_info(APP *app) {
  VkSwapchainCreateInfoKHR info = {0};
  VkSurfaceCapabilitiesKHR caps;
  VkPhysicalDevice physical = VK_CTX(app)->physical_device;
  VkSurfaceKHR surface = VK_CTX(app)->render_context.surface;
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps) != VK_SUCCESS) return info;
  uint32_t count = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, NULL) != VK_SUCCESS || !count) return info;
  VkSurfaceFormatKHR *formats = malloc((size_t)count * sizeof(*formats));
  if (!formats) return info;
  int width, height; platform_framebuffer_size(app->window, &width, &height);
  const char *capture_dir = getenv("APP_FRAME_DUMP_DIR");
  bool capture = capture_dir && *capture_dir;
  bool ok = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &count, formats) == VK_SUCCESS &&
            choose_surface(&caps, formats, count, width > 0 ? (uint32_t)width : 0,
                           height > 0 ? (uint32_t)height : 0, capture, &info);
  free(formats);
  if (!ok) return (VkSwapchainCreateInfoKHR){0};
  if (capture && !(info.imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    LOG_WARN("Frame capture unavailable for this surface; rendering remains enabled");
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.surface = surface; info.presentMode = choose_present_mode(app);
  return info;
}

bool create_image_views(APP *app) {
  Swapchain *swap = &VK_CTX(app)->render_context.swapchain;
  swap->image_views = calloc(swap->image_count, sizeof(*swap->image_views));
  if (!swap->image_views) return false;
  for (uint32_t i = 0; i < swap->image_count; ++i) {
    VkImageViewCreateInfo info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = swap->images[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = swap->format,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    if (vkCreateImageView(VK_CTX(app)->logical_device, &info, NULL, &swap->image_views[i]) != VK_SUCCESS) {
      swap->image_views[i] = VK_NULL_HANDLE; return false;
    }
  }
  return true;
}

Swapchain create_swapchain(APP *app) {
  Swapchain swap = {0};
  if (!app || !app->context || !app->window || !app->context->logical_device) return swap;
  VkDevice device = VK_CTX(app)->logical_device;
  VkSwapchainCreateInfoKHR info = create_swapchain_info(app);
  if (!info.sType || vkCreateSwapchainKHR(device, &info, NULL, &swap.handle) != VK_SUCCESS) return (Swapchain){0};
  swap.format = info.imageFormat; swap.present_mode = info.presentMode; swap.extent = info.imageExtent;
  swap.transfer_src = (info.imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
  if (vkGetSwapchainImagesKHR(device, swap.handle, &swap.image_count, NULL) != VK_SUCCESS || !swap.image_count) goto fail;
  swap.images = malloc((size_t)swap.image_count * sizeof(*swap.images));
  if (!swap.images || vkGetSwapchainImagesKHR(device, swap.handle, &swap.image_count, swap.images) != VK_SUCCESS) goto fail;
  int width, height; platform_window_size(app->window, &width, &height);
  g_virtualWidth = width > 0 ? (float)width : (float)swap.extent.width;
  g_virtualHeight = height > 0 ? (float)height : (float)swap.extent.height;
  swap.viewport_rect = (VkRect2D){.extent = swap.extent};
  return swap;
fail:
  free(swap.images); vkDestroySwapchainKHR(device, swap.handle, NULL);
  return (Swapchain){0};
}

void destroy_swapchain(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  Swapchain *swap = &VK_CTX(app)->render_context.swapchain;
  if (swap->image_views)
    for (uint32_t i = 0; i < swap->image_count; ++i)
      if (swap->image_views[i]) vkDestroyImageView(device, swap->image_views[i], NULL);
  free(swap->image_views); free(swap->images);
  if (swap->handle) vkDestroySwapchainKHR(device, swap->handle, NULL);
  *swap = (Swapchain){0};
}

void recreate_swapchain(APP *app) {
  if (!app || app->error.code) return;
  if (!app->context || !app->window || !app->context->logical_device) {
    app->error = (AppError){APP_ERROR_STATE, "swapchain recreation without device"};
    return;
  }
  int width = 0, height = 0;
  platform_framebuffer_size(app->window, &width, &height);
  while ((!width || !height) && !platform_window_should_close(app->window)) {
    platform_wait_events(); platform_framebuffer_size(app->window, &width, &height);
  }
  if (platform_window_should_close(app->window)) return;
  if (!vulkan_check(app, vkDeviceWaitIdle(VK_CTX(app)->logical_device), "swapchain recreation idle wait")) goto fail;
  destroy_framebuffers(app); destroy_pipeline(app); destroy_renderpass(app); destroy_swapchain(app);
  VK_CTX(app)->render_context.swapchain = create_swapchain(app);
  if (!VK_CTX(app)->render_context.swapchain.handle || !create_image_views(app) ||
      !sync_resize_swapchain_objects(app) || !create_renderpass(app) ||
      !create_pipeline(app) || !create_framebuffers(app)) goto fail;
  return;
fail:
  if (!app->error.code) app->error = (AppError){APP_ERROR_GRAPHICS, "swapchain recreation"};
  platform_window_set_should_close(app->window, PLATFORM_TRUE);
}

void vsync_toggle_and_apply(APP *app, bool enabled) {
  vsync_set_enabled(enabled); app->framebuffer_resized = true;
}
