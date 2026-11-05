#include <string.h>
#include "render/vulkan_draw.h"
#include "render/vulkan_error.h"
#include "render/vulkan_command.h"
#include "render/vulkan_swapchain.h"
#include "render/vulkan_vertex_buffer.h"
#include "render/frame_dump.h"

/* current_frame identifica uno slot riusabile di CPU/GPU, non un
 * indice immagine della swapchain. Le due forme di sincronizzazione sono distinte. */
bool wait_for_current_frame(APP *app) {
  if (!vulkan_frame_ready(app)) return false;
  RenderContext *rc = &VK_CTX(app)->render_context;
  VkDevice device = VK_CTX(app)->logical_device;
  uint32_t f = rc->current_frame;
  if (!vulkan_check(app, vkWaitForFences(device, 1, &rc->frames[f].in_flight_fence,
                                        VK_TRUE, UINT64_MAX), "frame fence wait")) return false;
  FrameTransferState *transfer = &rc->transfers[f];
  if (transfer->submitted) {
    // An upload can outlive an acquire that returned OUT_OF_DATE.
    if (!transfer->covered_by_render_fence &&
        !vulkan_check(app, vkWaitForFences(device, 1, &transfer->completion_fence,
                                          VK_TRUE, UINT64_MAX), "orphaned upload wait")) return false;
    // The upload path resets this fence immediately before its next submit.
    transfer->submitted = false;
    transfer->covered_by_render_fence = false;
  }
  frame_dump_collect(app, f);
  vertex_buffer_begin_frame(&app->renderer.gpu->hud_buffer, f);
  return true;
}

/* Pipeline del frame: upload -> acquisizione immagine -> registrazione comandi
 * -> submit -> present. false interrompe il loop; true puo anche indicare un
 * frame saltato durante la ricreazione della swapchain. */
bool draw_frame(APP *app) {
  if (!vulkan_frame_ready(app)) return false;
  RenderContext *rc = &VK_CTX(app)->render_context;
  VkDevice device = VK_CTX(app)->logical_device;
  uint32_t f = rc->current_frame;
  if (!app->renderer.gpu->camera_ubo[f].mapped || !rc->commands.buffers ||
      f >= rc->commands.count || !rc->swapchain.handle) {
    app->error = (AppError){APP_ERROR_STATE, "incomplete render frame resources"};
    return false;
  }
  app->renderer.camera_data.projection = mat4_ortho_world_z(0, VIRTUAL_W, 0, VIRTUAL_H, WORLD_Z_MIN, WORLD_Z_MAX);
  app->renderer.camera_data.view = mat4_identity();
  memcpy(app->renderer.gpu->camera_ubo[f].mapped, &app->renderer.camera_data, sizeof(CameraData));
  if (!submit_pending_uploads(app, f)) return false;

  uint32_t image_index = 0;
  VkResult result = vkAcquireNextImageKHR(device, rc->swapchain.handle, UINT64_MAX,
      rc->frames[f].image_available_semaphore, VK_NULL_HANDLE, &image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain(app);
    if (app->error.code) return false;
    rc->current_frame = (f + 1) % MAX_FRAMES_IN_FLIGHT;
    return true; // No image acquired, no graphics fence reset or submission.
  }
  bool suboptimal = result == VK_SUBOPTIMAL_KHR;
  if (!suboptimal && !vulkan_check(app, result, "swapchain image acquire")) return false;
  if (image_index >= rc->swapchain.image_count || !rc->images_in_flight ||
      !rc->render_finished_semaphores || image_index >= rc->render_finished_semaphore_count) {
    app->error = (AppError){APP_ERROR_STATE, "invalid acquired image index or synchronization"};
    return false;
  }
  if (rc->images_in_flight[image_index] != VK_NULL_HANDLE &&
      !vulkan_check(app, vkWaitForFences(device, 1, &rc->images_in_flight[image_index],
                                        VK_TRUE, UINT64_MAX), "image fence wait")) return false;

  VkCommandBuffer command = rc->commands.buffers[f];
  if (!vulkan_check(app, vkResetCommandBuffer(command, 0), "graphics command reset") ||
      !record_command_buffer(app, f, image_index)) return false;
  // Only reset when recording succeeded. A failed submit is fatal: never wait
  // on its now-unsignaled fence on another iteration of the loop.
  if (!vulkan_check(app, vkResetFences(device, 1, &rc->frames[f].in_flight_fence),
                    "graphics fence reset")) return false;
  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1, .pWaitSemaphores = &rc->frames[f].image_available_semaphore,
    .pWaitDstStageMask = &wait_stage, .commandBufferCount = 1, .pCommandBuffers = &command,
    .signalSemaphoreCount = 1, .pSignalSemaphores = &rc->render_finished_semaphores[image_index]
  };
  if (!vulkan_check(app, vkQueueSubmit(VK_CTX(app)->graphic_queue, 1, &submit,
                                      rc->frames[f].in_flight_fence), "graphics queue submit")) return false;
  // Associa l'immagine alla fence solo dopo un submit riuscito.
  rc->images_in_flight[image_index] = rc->frames[f].in_flight_fence;
  if (rc->transfers[f].submitted) rc->transfers[f].covered_by_render_fence = true;

  VkPresentInfoKHR present = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1, .pWaitSemaphores = &rc->render_finished_semaphores[image_index],
    .swapchainCount = 1, .pSwapchains = &rc->swapchain.handle, .pImageIndices = &image_index
  };
  result = vkQueuePresentKHR(VK_CTX(app)->graphic_queue, &present);
  bool needs_recreation = result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR;
  // A pending resize must not hide DEVICE_LOST or another fatal present error.
  if (!needs_recreation && !vulkan_check(app, result, "swapchain present")) return false;
  if (needs_recreation || suboptimal || app->framebuffer_resized) {
    app->framebuffer_resized = false;
    recreate_swapchain(app);
    if (app->error.code) return false;
  }
  rc->current_frame = (f + 1) % MAX_FRAMES_IN_FLIGHT;
  return true;
}
