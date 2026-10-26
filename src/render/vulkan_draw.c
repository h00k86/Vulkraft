#include <string.h>
#include "render/vulkan_draw.h"
#include "render/gfx_vulkan.h"

#include "log/log.h"
#include "render/vulkan_command.h"
#include "render/vulkan_swapchain.h"
#include "render/vulkan_vertex_buffer.h"
#include "render/frame_dump.h"

void wait_for_current_frame(APP *app) {
  RenderContext *rc = &VK_CTX(app)->render_context;
  VkDevice device = VK_CTX(app)->logical_device;
  uint32_t f = rc->current_frame;

  if (vkWaitForFences(device, 1, &rc->frames[f].in_flight_fence, VK_TRUE,
                      UINT64_MAX) != VK_SUCCESS) {
    LOG_ERROR("wait for current frame fence failed");
    exit(1);
  }
  frame_dump_collect(app, f); /* MACROTASK MULTI-GIOCO: dump diagnostico (APP_FRAME_DUMP_DIR) */
  FrameTransferState *transfer = &rc->transfers[f];
  if (transfer->submitted) {
    /* Normally the later graphics fence covers this same-queue upload. If
     * acquire returned OUT_OF_DATE before graphics submission, this bounded
     * exceptional wait is the remaining CPU ownership dependency. */
    if (!transfer->covered_by_render_fence) {

      VkResult transfer_wait = vkWaitForFences(
          device, 1, &transfer->completion_fence, VK_TRUE, UINT64_MAX);

      if (transfer_wait != VK_SUCCESS) {
        LOG_ERROR("wait for orphaned frame upload failed");
        exit(1);
      }
    }
    vkResetFences(device, 1, &transfer->completion_fence);
    transfer->submitted = false;
    transfer->covered_by_render_fence = false;
  }
  // This fence owns every per-frame staging/device vertex resource. Clearing
  // the upload marker here records the exact safe reuse boundary.
  vertex_buffer_begin_frame(&app->renderer.gpu->hud_buffer, f);
}

void draw_frame(APP *app) {
  RenderContext *rc = &VK_CTX(app)->render_context;
  VkDevice device = VK_CTX(app)->logical_device;
  uint32_t f = rc->current_frame;

  app->renderer.camera_data.projection = mat4_ortho_world_z(0, VIRTUAL_W, 0, VIRTUAL_H, WORLD_Z_MIN, WORLD_Z_MAX);
  app->renderer.camera_data.view = mat4_identity();
  memcpy(app->renderer.gpu->camera_ubo[f].mapped, &app->renderer.camera_data, sizeof(CameraData));

  /* Upload first. The engine's transfer and graphics work uses the same
   * VkQueue, so submission order is the GPU dependency; the CPU can proceed
   * into acquire without waiting for the copy fence. */
  if (!submit_pending_uploads(app, f))
    exit(1);

  uint32_t image_index;

  VkResult result = vkAcquireNextImageKHR(
      device, rc->swapchain.handle, UINT64_MAX,
      rc->frames[f].image_available_semaphore, VK_NULL_HANDLE, &image_index);


  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain(app);
    // Avanza comunque lo slot, PRIMA di uscire: Vulkan non garantisce
    // nulla sullo stato di image_available_semaphore quando
    // l'acquisizione fallisce con OUT_OF_DATE — alcuni driver lo
    // segnalano comunque. Se restassimo sullo stesso slot 'f', il
    // prossimo vkAcquireNextImageKHR proverebbe a segnalare QUESTO
    // stesso semaforo una seconda volta senza che nessuno lo abbia mai
    // atteso in mezzo — non permesso per i semafori binari (devono
    // alternare rigorosamente segnala/attendi). Passare allo slot
    // successivo evita il problema alla radice.
    rc->current_frame = (f + 1) % MAX_FRAMES_IN_FLIGHT;
    return;
  }

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOG_ERROR("vkAcquireNextImageKHR failed: %d", result);
    exit(1);
  }

  // vkAcquireNextImageKHR NON garantisce di restituire le immagini nello
  // stesso ordine round-robin di rc->current_frame — anche con
  // MAX_FRAMES_IN_FLIGHT allineato al numero di immagini della swapchain
  // (qui 3 e 3), l'immagine restituita può non corrispondere allo slot
  // di frame 'f' che si sta usando in questo momento, se è ancora in
  // volo da un frame-slot DIVERSO, sottomesso in un giro precedente.
  // Senza questo controllo, si rischia di registrare/sottomettere un
  // nuovo comando per un'immagine che la GPU sta ancora presentando da
  // un submit precedente — non garantito dalla spec, possibile fonte di
  // artefatti intermittenti difficili da riprodurre.
  if (rc->images_in_flight[image_index] != VK_NULL_HANDLE) {

    vkWaitForFences(device, 1, &rc->images_in_flight[image_index], VK_TRUE,
                    UINT64_MAX);

  }
  rc->images_in_flight[image_index] = rc->frames[f].in_flight_fence;

  // 2. Reset della fence SOLO dopo aver superato il check OUT_OF_DATE
  vkResetFences(device, 1, &rc->frames[f].in_flight_fence);

  // 3. REGISTRAZIONE: Usa rc->commands.buffers[f] invece di image_index!
  // Poiché abbiamo aspettato la fence di 'f', siamo sicuri che buffers[f] sia
  // libero.
  VkCommandBuffer current_cmd = rc->commands.buffers[f];

  vkResetCommandBuffer(current_cmd, 0);

  record_command_buffer(app, f, image_index);


  VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &rc->frames[f].image_available_semaphore,
      .pWaitDstStageMask =
          (VkPipelineStageFlags[]){
              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
      .commandBufferCount = 1,
      .pCommandBuffers = &current_cmd, // Passiamo il buffer dello slot f
      .signalSemaphoreCount = 1,
      // Per IMMAGINE, non per frame: la presentazione tiene occupato questo
      // semaforo finche' l'immagine non viene ri-acquisita, e gli indici
      // immagine non seguono gli slot di frame. Vedi vulkan_sync.c.
      .pSignalSemaphores = &rc->render_finished_semaphores[image_index]};

  VkResult submitResult = vkQueueSubmit(VK_CTX(app)->graphic_queue, 1,
                                        &submitInfo,
                                        rc->frames[f].in_flight_fence);

  if (submitResult != VK_SUCCESS) {
    LOG_ERROR("Submit failed!");
    exit(1);
  }
  if (rc->transfers[f].submitted)
    rc->transfers[f].covered_by_render_fence = true;

  VkPresentInfoKHR presentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                  .waitSemaphoreCount = 1,
                                  .pWaitSemaphores =
                                      &rc->render_finished_semaphores[image_index],
                                  .swapchainCount = 1,
                                  .pSwapchains = &rc->swapchain.handle,
                                  .pImageIndices = &image_index};

  result = vkQueuePresentKHR(VK_CTX(app)->graphic_queue, &presentInfo);


  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      app->framebuffer_resized) {
    app->framebuffer_resized = false;
    recreate_swapchain(app);
  } else if (result != VK_SUCCESS) {
    LOG_ERROR("vkQueuePresentKHR failed: %d", result);
    exit(1);
  }

  rc->current_frame = (f + 1) % MAX_FRAMES_IN_FLIGHT;
}
