#include "app/application.h"
#include "render/gfx_vulkan.h"
#include "render/vulkan_swapchain.h"
#include "render/vulkan_sync.h"
#include "render/vsync_settings.h"
#include "render/vulkan_framebuffer.h"
#include "render/vulkan_pipeline.h"
#include "render/vulkan_uniform_buffer.h"
#include "log/log.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>

static VkPresentModeKHR choose_present_mode(APP *app) {
  /* test only (2026-10-16): APP_NO_VSYNC=1 per i fotogrammi di prova con
   * lo schermo spento (FIFO aspetta il monitor: un fotogramma al secondo) */
  if (vsync_get_enabled() && !getenv("APP_NO_VSYNC"))
    return VK_PRESENT_MODE_FIFO_KHR;

  uint32_t modeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      VK_CTX(app)->physical_device, VK_CTX(app)->render_context.surface, &modeCount,
      NULL);

  if (modeCount == 0)
    return VK_PRESENT_MODE_FIFO_KHR; // difesa: nessuna modalità
                                     // riportata, resta sul sicuro

  VkPresentModeKHR modes[16];
  if (modeCount > 16)
    modeCount = 16; // difesa: più di 16 modalità è oltre qualunque
                    // driver reale conosciuto, non serve un malloc
                    // dinamico per questo
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      VK_CTX(app)->physical_device, VK_CTX(app)->render_context.surface, &modeCount,
      modes);

  bool hasMailbox = false, hasImmediate = false;
  for (uint32_t i = 0; i < modeCount; i++) {
    if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
      hasMailbox = true;
    else if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
      hasImmediate = true;
  }

  if (hasMailbox)
    return VK_PRESENT_MODE_MAILBOX_KHR;
  if (hasImmediate)
    return VK_PRESENT_MODE_IMMEDIATE_KHR;

  LOG_WARN("V-Sync spento richiesto ma il driver non supporta MAILBOX né "
           "IMMEDIATE — resto su FIFO (V-Sync di fatto acceso)");
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkSwapchainCreateInfoKHR create_swapchain_info(APP *app) {

  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      VK_CTX(app)->physical_device, VK_CTX(app)->render_context.surface, &capabilities);

  VkSwapchainCreateInfoKHR swapInfo = {0};
  swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

  swapInfo.surface = VK_CTX(app)->render_context.surface;

  // Prima era un "3" fisso, mai verificato contro ciò che il driver
  // supporta davvero (appena letto sopra in 'capabilities', poi
  // ignorato). Su un driver/GPU con un range diverso (es. solo 2
  // immagini) vkCreateSwapchainKHR può fallire o comportarsi in modo
  // imprevisto. maxImageCount == 0 significa "nessun limite" per spec,
  // va trattato come tale, non come "zero".
  uint32_t desiredImageCount = 3; // minimo safe che volevamo comunque
  if (desiredImageCount < capabilities.minImageCount)
    desiredImageCount = capabilities.minImageCount;
  if (capabilities.maxImageCount > 0 &&
      desiredImageCount > capabilities.maxImageCount)
    desiredImageCount = capabilities.maxImageCount;
  swapInfo.minImageCount = desiredImageCount;

  swapInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
  swapInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

  // currentExtent.width == 0xFFFFFFFF è il valore sentinella con cui
  // alcune piattaforme (tipicamente Wayland) dicono "decidi tu le
  // dimensioni" — usato direttamente (come prima) crea una swapchain con
  // extent invalida lì. In quel caso si chiede a GLFW la dimensione vera
  // del framebuffer e la si clampa dentro i limiti min/max del driver.
  VkExtent2D extent = capabilities.currentExtent;
  if (extent.width == 0xFFFFFFFFu) {
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(app->window, &fbW, &fbH);
    extent.width = (uint32_t)fbW;
    extent.height = (uint32_t)fbH;

    if (extent.width < capabilities.minImageExtent.width)
      extent.width = capabilities.minImageExtent.width;
    if (extent.width > capabilities.maxImageExtent.width)
      extent.width = capabilities.maxImageExtent.width;
    if (extent.height < capabilities.minImageExtent.height)
      extent.height = capabilities.minImageExtent.height;
    if (extent.height > capabilities.maxImageExtent.height)
      extent.height = capabilities.maxImageExtent.height;
  }
  swapInfo.imageExtent = extent;
  swapInfo.imageArrayLayers = 1;
  // MACROTASK MULTI-GIOCO: TRANSFER_SRC per il dump diagnostico del frame
  // (APP_FRAME_DUMP_DIR, render/vulkan_draw.c) -- letto in host memory
  // dopo la present, mai nel percorso normale.
  swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

  swapInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

  swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

  swapInfo.presentMode = choose_present_mode(app);

  swapInfo.clipped = VK_TRUE;

  return swapInfo;
}

void create_image_views(APP *app) {

  Swapchain *swap = &VK_CTX(app)->render_context.swapchain;

  swap->image_views = malloc(sizeof(VkImageView) * swap->image_count);

  for (uint32_t i = 0; i < swap->image_count; i++) {

    VkImageViewCreateInfo viewInfo = {0};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    viewInfo.image = swap->images[i];

    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = swap->format; // stesso della swapchain

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(VK_CTX(app)->logical_device, &viewInfo, NULL,
                          &swap->image_views[i]) != VK_SUCCESS) {

      LOG_ERROR("failed to create image view");
      exit(1);
    }
  }

  LOG_INFO("image views created");
}

Swapchain create_swapchain(APP *app) {

  LOG_INFO("creating swapchain");

  Swapchain swap = {0};

  VkSwapchainCreateInfoKHR swapInfo = create_swapchain_info(app);

  if (vkCreateSwapchainKHR(VK_CTX(app)->logical_device, &swapInfo, NULL,
                           &swap.handle) != VK_SUCCESS) {

    LOG_ERROR("error creating swapchain");
    exit(123);
  }

  // 🔥 salva info utili
  swap.format = swapInfo.imageFormat;
  swap.present_mode = swapInfo.presentMode;
  swap.extent = swapInfo.imageExtent;
  {
    int width = 0, height = 0;
    glfwGetWindowSize(app->window, &width, &height);
    g_virtualWidth = width > 0 ? (float)width : (float)swap.extent.width;
    g_virtualHeight = height > 0 ? (float)height : (float)swap.extent.height;
    swap.viewport_rect = (VkRect2D){.offset = {0, 0}, .extent = swap.extent};
    LOG_INFO("Application surface %ux%u, logical %.0fx%.0f", swap.extent.width, swap.extent.height, VIRTUAL_W, VIRTUAL_H);
  }

  // 🔥 ottieni numero immagini
  vkGetSwapchainImagesKHR(VK_CTX(app)->logical_device, swap.handle,
                          &swap.image_count, NULL);

  // 🔥 malloc immagini
  swap.images = malloc(sizeof(VkImage) * swap.image_count);

  vkGetSwapchainImagesKHR(VK_CTX(app)->logical_device, swap.handle,
                          &swap.image_count, swap.images);

  LOG_INFO("swapchain images: %d", swap.image_count);
  return swap;
}

void destroy_swapchain(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  Swapchain *sc = &VK_CTX(app)->render_context.swapchain; // NOME CORRETTO

  if (sc->image_views) {
    for (uint32_t i = 0; i < sc->image_count; i++) {
      vkDestroyImageView(device, sc->image_views[i], NULL);
    }
    free(sc->image_views);
    sc->image_views = NULL;
  }

  if (sc->images) {
    free(sc->images);
    sc->images = NULL;
  }

  // NON liberare ctx->images_in_flight qui!
  vkDestroySwapchainKHR(device, sc->handle, NULL);
  LOG_INFO("destroyed swapchain");
}

void recreate_swapchain(APP *app) {
  int width = 0, height = 0;
  glfwGetFramebufferSize(app->window, &width, &height);
  while ((width == 0 || height == 0) && !glfwWindowShouldClose(app->window)) {
    glfwWaitEvents();
    glfwGetFramebufferSize(app->window, &width, &height);
  }
  if (glfwWindowShouldClose(app->window)) return;
  vkDeviceWaitIdle(VK_CTX(app)->logical_device);
  destroy_framebuffers(app);
  destroy_pipeline(app);
  destroy_swapchain(app);
  VK_CTX(app)->render_context.swapchain = create_swapchain(app);
  create_image_views(app);
  sync_resize_swapchain_objects(app);
  create_pipeline(app);
  create_framebuffers(app);
}

void vsync_toggle_and_apply(APP *app, bool enabled) {
  vsync_set_enabled(enabled);
  app->framebuffer_resized = true;
}
