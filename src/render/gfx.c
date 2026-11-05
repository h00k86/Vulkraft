#include "render/gfx.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "render/init_vulkan.h"
#include "render/vulkan_swapchain.h"
#include "render/vulkan_physical_device.h"
#include "log/log.h"
#include <stdlib.h>

uint32_t gfx_current_frame(const APP *app) {
  if (!app || !app->context)
    return 0;
  return VK_CTX((APP *)app)->render_context.current_frame;
}

bool gfx_device_ready(const APP *app) {
  return app && app->context &&
         VK_CTX((APP *)app)->logical_device != VK_NULL_HANDLE;
}

void gfx_wait_idle(APP *app) {
  if (gfx_device_ready(app))
    vkDeviceWaitIdle(VK_CTX((APP *)app)->logical_device);
}

bool gfx_init(APP *app) {
  if (!app) return false;
  if (app->context || app->renderer.gpu || app->window) {
    app->error = (AppError){APP_ERROR_STATE, "graphics already initialized"};
    return false;
  }
  app->context = calloc(1, sizeof(VulkanContext));
  app->renderer.gpu = calloc(1, sizeof(RendererGpu));
  if (!app->context || !app->renderer.gpu) {
    app->error = (AppError){APP_ERROR_MEMORY, "graphics state allocation"};
    gfx_shutdown(app);
    return false;
  }
  if (!init_vulkan(app)) {
    gfx_shutdown(app);
    return false;
  }
  app->error = (AppError){0};
  return true;
}

void gfx_shutdown(APP *app) {
  if (!app)
    return;
  destroy_vulkan(app);
  free(app->renderer.gpu);
  app->renderer.gpu = NULL;
  free(app->context);
  app->context = NULL;
}

void gfx_recreate_surface(APP *app) {
  if (app)
    recreate_swapchain(app);
}

void gfx_surface_size(const APP *app, uint32_t *w, uint32_t *h) {
  if (w) *w = 0;
  if (h) *h = 0;
  if (!app || !app->context)
    return;
  const Swapchain *sc = &VK_CTX((APP *)app)->render_context.swapchain;
  if (w) *w = sc->extent.width;
  if (h) *h = sc->extent.height;
}

const char *gfx_selected_gpu_name(void) { return gpu_selected_name(); }
