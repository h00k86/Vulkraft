#include "render/vulkan_sync.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "log/log.h"
#include <stdlib.h>

void create_sync_objects(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  RenderContext *ctx = &VK_CTX(app)->render_context;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VkSemaphoreCreateInfo semInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    vkCreateSemaphore(device, &semInfo, NULL,
                      &ctx->frames[i].image_available_semaphore);
    vkCreateFence(device, &fenceInfo, NULL, &ctx->frames[i].in_flight_fence);
    vkCreateFence(device, &fenceInfo, NULL,
                  &ctx->transfers[i].completion_fence);
    ctx->transfers[i].submitted = false;
    ctx->transfers[i].covered_by_render_fence = false;
  }

  sync_resize_swapchain_objects(app);
}

// Perche' il semaforo "render finito" e' per IMMAGINE e non per frame.
//
// vkQueuePresentKHR lo attende per una specifica immagine, e la
// presentazione lo tiene occupato finche' quell'immagine non viene
// RI-ACQUISITA. Ma vkAcquireNextImageKHR non restituisce le immagini
// nell'ordine degli slot di frame: con un semaforo per slot si finisce a
// segnalarne uno che una presentazione precedente sta ancora aspettando.
//
// Non e' teoria: i layer di validazione lo segnalavano ad ogni sessione con
//
//   VUID-vkQueueSubmit-pSignalSemaphores-00067
//   pSignalSemaphores[0] is being signaled by VkQueue, but it may still be
//   in use by VkSwapchainKHR.
//   Most recently acquired image indices: 3, 0, 1, [2], 3, 0, 1, 3.
//   Swapchain image 2 was presented but was not re-acquired.
//
// Indicizzando per immagine il problema sparisce alla radice: il semaforo
// dell'immagine N puo' essere ri-segnalato solo quando stiamo acquisendo di
// nuovo N, cioe' quando la presentazione precedente di N ha gia' consumato
// la sua attesa. E' la soluzione (a) suggerita dal layer stesso.
//
// Fence e semafori sono dimensionati sulla swapchain, quindi vanno
// riallocati ad ogni ricreazione: il numero di immagini cambia (3 con FIFO
// all'avvio, 4 con MAILBOX/IMMEDIATE). Scrivere all'indice 3 di un array da
// 3 e' un overflow vero — era gia' successo con images_in_flight.
void sync_resize_swapchain_objects(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  RenderContext *ctx = &VK_CTX(app)->render_context;

  if (ctx->render_finished_semaphores) {
    for (uint32_t i = 0; i < ctx->render_finished_semaphore_count; i++)
      vkDestroySemaphore(device, ctx->render_finished_semaphores[i], NULL);
    free(ctx->render_finished_semaphores);
    ctx->render_finished_semaphores = NULL;
  }
  ctx->render_finished_semaphore_count = 0;

  if (ctx->images_in_flight) {
    free(ctx->images_in_flight);
    ctx->images_in_flight = NULL;
  }

  if (ctx->swapchain.image_count == 0) {
    LOG_ERROR("cannot allocate per-image sync objects for an empty swapchain");
    exit(1);
  }

  ctx->images_in_flight = calloc(ctx->swapchain.image_count, sizeof(VkFence));
  if (!ctx->images_in_flight) {
    LOG_ERROR("cannot allocate images_in_flight");
    exit(1);
  }

  ctx->render_finished_semaphores =
      calloc(ctx->swapchain.image_count, sizeof(VkSemaphore));
  if (!ctx->render_finished_semaphores) {
    LOG_ERROR("cannot allocate render_finished_semaphores");
    exit(1);
  }

  VkSemaphoreCreateInfo semInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for (uint32_t i = 0; i < ctx->swapchain.image_count; i++) {
    if (vkCreateSemaphore(device, &semInfo, NULL,
                          &ctx->render_finished_semaphores[i]) != VK_SUCCESS) {
      LOG_ERROR("cannot create render finished semaphore %u", i);
      exit(1);
    }
  }
  ctx->render_finished_semaphore_count = ctx->swapchain.image_count;
}

void destroy_sync_objects(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  RenderContext *ctx = &VK_CTX(app)->render_context;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(device, ctx->frames[i].image_available_semaphore, NULL);
    vkDestroyFence(device, ctx->frames[i].in_flight_fence, NULL);
    vkDestroyFence(device, ctx->transfers[i].completion_fence, NULL);
  }

  // LIBERAZIONE UNICA
  if (ctx->render_finished_semaphores) {
    for (uint32_t i = 0; i < ctx->render_finished_semaphore_count; i++)
      vkDestroySemaphore(device, ctx->render_finished_semaphores[i], NULL);
    free(ctx->render_finished_semaphores);
    ctx->render_finished_semaphores = NULL;
  }
  ctx->render_finished_semaphore_count = 0;
  if (ctx->images_in_flight) {
    free(ctx->images_in_flight);
    ctx->images_in_flight = NULL;
  }
}
