#include "render/vulkan_command.h"
#include "render/gfx_vulkan.h"
#include "render/vulkan_error.h"
#include "app/application.h"
#include "render/vulkan_vertex_buffer.h"
#include "render/text_renderer.h"
#include "render/frame_dump.h"
#include <stdlib.h>
#include <string.h>

bool create_command_buffers(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  RenderContext *ctx = &VK_CTX(app)->render_context;

  // Usiamo MAX_FRAMES_IN_FLIGHT per i buffer, così ogni frame "in volo" ne ha
  // uno
  ctx->commands.count = MAX_FRAMES_IN_FLIGHT;

  VkCommandPoolCreateInfo poolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = VK_CTX(app)->family_queue_index,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};

  if (vkCreateCommandPool(device, &poolInfo, NULL, &ctx->commands.pool) != VK_SUCCESS) {
    ctx->commands.pool = VK_NULL_HANDLE; return false;
  }

  ctx->commands.buffers =
      calloc(MAX_FRAMES_IN_FLIGHT, sizeof(VkCommandBuffer));
  if (!ctx->commands.buffers) return false;

  VkCommandBufferAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = ctx->commands.pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

  if (vkAllocateCommandBuffers(device, &alloc, ctx->commands.buffers) != VK_SUCCESS) return false;

  VkCommandBuffer upload_commands[MAX_FRAMES_IN_FLIGHT];
  if (vkAllocateCommandBuffers(device, &alloc, upload_commands) != VK_SUCCESS) return false;
  for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
    ctx->transfers[frame].command_buffer = upload_commands[frame];
  return true;
}

/* Registra le copie dello slot corrente e la barriera verso l'uso come vertici.
 * Un trasferimento inviato prima di un acquire fallito deve essere atteso
 * anche se nessun submit grafico successivo lo coprira con la propria fence. */
bool submit_pending_uploads(APP *app, uint32_t frame_index) {
  if (!vulkan_frame_ready(app)) return false;
  if (frame_index >= MAX_FRAMES_IN_FLIGHT) {
    app->error = (AppError){APP_ERROR_STATE, "invalid command frame index"};
    return false;
  }
  RenderContext *ctx = &VK_CTX(app)->render_context;
  FrameTransferState *transfer = &ctx->transfers[frame_index];
  VkCommandBuffer cmd = transfer->command_buffer;
  if (!cmd) {
    app->error = (AppError){APP_ERROR_STATE, "missing upload command buffer"};
    return false;
  }

  if (!vulkan_check(app, vkResetCommandBuffer(cmd, 0), "upload command reset")) return false;
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  if (!vulkan_check(app, vkBeginCommandBuffer(cmd, &begin_info), "upload command begin")) {
    LOG_ERROR("upload command buffer begin failed for frame %u", frame_index);
    return false;
  }

  VkBufferMemoryBarrier barrier;
  VertexBuffer *vb = &app->renderer.gpu->hud_buffer;
  bool has_upload = vertex_buffer_record_upload(vb, frame_index, cmd, &barrier);
  if (!has_upload && vb->upload_requested[frame_index]) {
    app->error = (AppError){APP_ERROR_STATE, "invalid vertex upload range"};
    return false;
  }
  if (has_upload) {
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, NULL,
                         1, &barrier, 0, NULL);
  }
  if (!vulkan_check(app, vkEndCommandBuffer(cmd), "upload command end")) {
    LOG_ERROR("upload command buffer end failed for frame %u", frame_index);
    return false;
  }

  if (!has_upload) {
    return true;
  }

  if (!vulkan_check(app, vkResetFences(VK_CTX(app)->logical_device, 1,
                &transfer->completion_fence), "upload fence reset")) return false;
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
  };

  VkResult result = vkQueueSubmit(VK_CTX(app)->graphic_queue, 1, &submit_info,
                                  transfer->completion_fence);

  if (!vulkan_check(app, result, "upload queue submit")) {
    LOG_ERROR("upload queue submit failed for frame %u: %d", frame_index,
              result);
    return false;
  }
  transfer->submitted = true;
  transfer->covered_by_render_fence = false;
  return true;
}

/* Registra il disegno senza inviarlo alla coda. La fence grafica verra
 * resettata dal chiamante soltanto dopo una registrazione riuscita. */
bool record_command_buffer(APP *app, uint32_t frame_index, uint32_t image_index) {
  if (!vulkan_frame_ready(app)) return false;
  if (frame_index >= MAX_FRAMES_IN_FLIGHT) {
    app->error = (AppError){APP_ERROR_STATE, "invalid command frame index"};
    return false;
  }
  RenderContext *ctx = &VK_CTX(app)->render_context;
  if (!ctx->commands.buffers || frame_index >= ctx->commands.count ||
      image_index >= ctx->swapchain.image_count || !ctx->swapchain.framebuffers) {
    app->error = (AppError){APP_ERROR_STATE, "invalid graphics command resources"};
    return false;
  }
  VkCommandBuffer cmd = ctx->commands.buffers[frame_index];
  VertexBuffer *vb_hud = &app->renderer.gpu->hud_buffer;
  VkDeviceSize offset = 0;
  VkRect2D canvas_scissor = ctx->swapchain.viewport_rect;
  VkViewport viewport = {0, 0, (float)ctx->swapchain.extent.width, (float)ctx->swapchain.extent.height, 0, 1};
  VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  if (!vulkan_check(app, vkBeginCommandBuffer(cmd, &beginInfo), "graphics command begin")) return false;
  VkClearValue clearValues = {.color = {{0.008f, 0.009f, 0.012f, 1.0f}}};
  VkRenderPassBeginInfo rpInfo2 = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = ctx->renderpass,
    .framebuffer = ctx->swapchain.framebuffers[image_index],
    .renderArea = canvas_scissor,
    .clearValueCount = 1, .pClearValues = &clearValues
  };
  vkCmdBeginRenderPass(cmd, &rpInfo2, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &canvas_scissor);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app->renderer.gpu->pipeline.handle);
  // Draw the current UI geometry, respecting each batch clip rectangle.
  if (vb_hud->vertex_count > 0) {
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            app->renderer.gpu->pipeline.layout, 0, 1,
                            &app->renderer.gpu->hud_descriptor_set[frame_index], 0,
                            NULL);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb_hud->gpu_buffer[frame_index],
                          &offset);
    for (int i = 0; i < text_hud_batch_count(); ++i) {
      const HUDDrawBatch *batch = text_hud_batch_at(i);
      if (!batch || batch->vertex_count <= 0 || batch->clip_w <= 0.0f ||
          batch->clip_h <= 0.0f)
        continue;
      float sx = (float)canvas_scissor.extent.width / VIRTUAL_W;
      float sy = (float)canvas_scissor.extent.height / VIRTUAL_H;
      VkRect2D scissor = {
          .offset = {canvas_scissor.offset.x + (int32_t)(batch->clip_x * sx),
                     canvas_scissor.offset.y + (int32_t)(batch->clip_y * sy)},
          .extent = {(uint32_t)(batch->clip_w * sx),
                     (uint32_t)(batch->clip_h * sy)}};
      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdDraw(cmd, (uint32_t)batch->vertex_count, 1,
                (uint32_t)batch->first_vertex, 0);
    }
    vkCmdSetScissor(cmd, 0, 1, &canvas_scissor);
  }

  vkCmdEndRenderPass(cmd);
  frame_dump_record(app, cmd, frame_index, image_index);
  if (!vulkan_check(app, vkEndCommandBuffer(cmd), "graphics command end")) return false;
  return true;
}

void destroy_command_buffers(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  CommandSystem *cs = &VK_CTX(app)->render_context.commands;

  vkDestroyCommandPool(device, cs->pool, NULL);
  free(cs->buffers);
  *cs = (CommandSystem){0};
}
