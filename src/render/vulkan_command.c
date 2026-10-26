#include "render/vulkan_command.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "render/vulkan_vertex_buffer.h"
#include "render/text_renderer.h"
#include "render/frame_dump.h"
#include <stdlib.h>
#include <string.h>
static RenderCommandStats g_command_stats;

void create_command_buffers(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  RenderContext *ctx = &VK_CTX(app)->render_context;

  // Usiamo MAX_FRAMES_IN_FLIGHT per i buffer, così ogni frame "in volo" ne ha
  // uno
  ctx->commands.count = MAX_FRAMES_IN_FLIGHT;

  VkCommandPoolCreateInfo poolInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = VK_CTX(app)->family_queue_index,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT};

  vkCreateCommandPool(device, &poolInfo, NULL, &ctx->commands.pool);

  ctx->commands.buffers =
      malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = ctx->commands.pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

  vkAllocateCommandBuffers(device, &alloc, ctx->commands.buffers);

  VkCommandBuffer upload_commands[MAX_FRAMES_IN_FLIGHT];
  vkAllocateCommandBuffers(device, &alloc, upload_commands);
  for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
    ctx->transfers[frame].command_buffer = upload_commands[frame];

}

bool submit_pending_uploads(APP *app, uint32_t frame_index) {
  RenderContext *ctx = &VK_CTX(app)->render_context;
  FrameTransferState *transfer = &ctx->transfers[frame_index];
  VkCommandBuffer cmd = transfer->command_buffer;
  RenderCommandStats stats = {0};
  VertexBuffer *upload_buffers[1] = {
      &app->renderer.gpu->hud_buffer,
  };

  vkResetCommandBuffer(cmd, 0);
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
    LOG_ERROR("upload command buffer begin failed for frame %u", frame_index);
    g_command_stats = stats;
    return false;
  }

  VkBufferMemoryBarrier barriers[1];
  uint32_t upload_count = 0;
  for (uint32_t i = 0; i < 1; ++i) {
    VertexBuffer *vb = upload_buffers[i];
    if (vertex_buffer_record_upload(vb, frame_index, cmd,
                                    &barriers[upload_count])) {
      stats.transfer_bytes += vb->upload_size[frame_index];
      upload_count++;
    }
  }
  if (upload_count > 0) {
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, NULL,
                         upload_count, barriers, 0, NULL);
  }
  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
    LOG_ERROR("upload command buffer end failed for frame %u", frame_index);
    g_command_stats = stats;
    return false;
  }

  stats.upload_copies = upload_count;
  if (upload_count == 0) {
    g_command_stats = stats;
    return true;
  }

  vkResetFences(VK_CTX(app)->logical_device, 1,
                &transfer->completion_fence);
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
  };

  VkResult result = vkQueueSubmit(VK_CTX(app)->graphic_queue, 1, &submit_info,
                                  transfer->completion_fence);

  if (result != VK_SUCCESS) {
    LOG_ERROR("upload queue submit failed for frame %u: %d", frame_index,
              result);
    g_command_stats = stats;
    return false;
  }
  transfer->submitted = true;
  transfer->covered_by_render_fence = false;
  stats.transfer_submissions = 1;
  g_command_stats = stats;
  return true;
}

void record_command_buffer(APP *app, uint32_t frame_index, uint32_t image_index) {
  RenderContext *ctx = &VK_CTX(app)->render_context;
  VkCommandBuffer cmd = ctx->commands.buffers[frame_index];
  VertexBuffer *vb_hud = &app->renderer.gpu->hud_buffer;
  RenderCommandStats stats = g_command_stats;
  VkDeviceSize offset = 0;
  VkRect2D worldScissor = ctx->swapchain.viewport_rect;
  VkViewport viewport = {0, 0, (float)ctx->swapchain.extent.width, (float)ctx->swapchain.extent.height, 0, 1};
  VkCommandBufferBeginInfo beginInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) exit(1);
  VkClearValue clearValues = {.color = {{0.008f, 0.009f, 0.012f, 1.0f}}};
  VkRenderPassBeginInfo rpInfo2 = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = ctx->renderpass,
    .framebuffer = ctx->swapchain.framebuffers[image_index],
    .renderArea = worldScissor,
    .clearValueCount = 1, .pClearValues = &clearValues
  };
  vkCmdBeginRenderPass(cmd, &rpInfo2, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &worldScissor);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app->renderer.gpu->pipeline.handle);
  // --- DISEGNO HUD (Statico/UI) --- fresco, sopra a tutto, MAI toccato
  // dall'effetto acqua — vedi nota sopra sul perché non fa parte del
  // passaggio 1.
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
      float sx = (float)worldScissor.extent.width / VIRTUAL_W;
      float sy = (float)worldScissor.extent.height / VIRTUAL_H;
      VkRect2D scissor = {
          .offset = {worldScissor.offset.x + (int32_t)(batch->clip_x * sx),
                     worldScissor.offset.y + (int32_t)(batch->clip_y * sy)},
          .extent = {(uint32_t)(batch->clip_w * sx),
                     (uint32_t)(batch->clip_h * sy)}};
      vkCmdSetScissor(cmd, 0, 1, &scissor);
      vkCmdDraw(cmd, (uint32_t)batch->vertex_count, 1,
                (uint32_t)batch->first_vertex, 0);
      stats.draw_calls++;
      stats.hud_draw_calls++;
    }
    vkCmdSetScissor(cmd, 0, 1, &worldScissor);
  }


  vkCmdEndRenderPass(cmd);
  frame_dump_record(app, cmd, frame_index, image_index);
  if (vkEndCommandBuffer(cmd) != VK_SUCCESS) exit(1);
  g_command_stats = stats;
}

void destroy_command_buffers(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  CommandSystem *cs = &VK_CTX(app)->render_context.commands;


  vkDestroyCommandPool(device, cs->pool, NULL);
  free(cs->buffers);
}

RenderCommandStats render_command_get_stats(void) { return g_command_stats; }
