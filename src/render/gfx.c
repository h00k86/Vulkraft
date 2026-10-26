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

void gfx_init(APP *app) {
  if (!app)
    return;
  /* PORTING (E14, 2026-09-25): il contesto del backend e' un blocco a
   * parte, allocato QUI. Prima stava dentro APP per valore e non serviva
   * allocarlo; quando e' diventato un puntatore, per un momento nessuno lo
   * allocava piu' e ogni scrittura finiva nel contesto di riserva di
   * VK_CTX -- che e' `static inline`, cioe' UNO PER FILE: vulkan_instance.c
   * scriveva nella sua copia e vulkan_surface.c leggeva dalla propria,
   * trovandola vuota. Il sintomo era un assert di GLFW
   * ("instance != VK_NULL_HANDLE") al primo avvio del gioco impacchettato. */
  if (!app->context) {
    app->context = (VulkanContext *)calloc(1, sizeof(VulkanContext));
    if (!app->context) {
      LOG_ERROR("Rendering: memoria insufficiente per il contesto del backend");
      return;
    }
  }
  /* Stesso discorso per lo stato GPU del Renderer: sopra il confine e' solo
   * un puntatore opaco, e qualcuno deve allocarlo. calloc, non malloc: la
   * distruzione passa su ogni handle e uno non inizializzato non si
   * distingue da uno valido. */
  if (!app->renderer.gpu) {
    app->renderer.gpu = (RendererGpu *)calloc(1, sizeof(RendererGpu));
    if (!app->renderer.gpu) {
      LOG_ERROR("Rendering: memoria insufficiente per lo stato GPU");
      return;
    }
  }
  init_vulkan(app);
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
