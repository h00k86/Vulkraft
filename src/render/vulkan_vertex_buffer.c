#include "render/vulkan_vertex_buffer.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"

#include "log/log.h"
#include "render/vulkan_buffer.h"
#include "render/vulkan_error.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool vertex_capacity_bytes(uint32_t capacity, VkDeviceSize *out_size) {
  /* VkDeviceSize is uint64_t and capacity is uint32_t, so this product
   * cannot overflow for the compile-time Vertex size used by this binary. */
  if (!out_size || capacity == 0)
    return false;
  *out_size = (VkDeviceSize)capacity * (VkDeviceSize)sizeof(Vertex);
  return true;
}

static void destroy_storage_set(
    VkDevice device, VkBuffer gpu[MAX_FRAMES_IN_FLIGHT],
    VkDeviceMemory gpu_memory[MAX_FRAMES_IN_FLIGHT],
    VkBuffer staging[MAX_FRAMES_IN_FLIGHT],
    VkDeviceMemory staging_memory[MAX_FRAMES_IN_FLIGHT],
    void *mapped[MAX_FRAMES_IN_FLIGHT]) {
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (mapped[i] && staging_memory[i])
      vkUnmapMemory(device, staging_memory[i]);
    if (staging[i])
      vkDestroyBuffer(device, staging[i], NULL);
    if (staging_memory[i])
      vkFreeMemory(device, staging_memory[i], NULL);
    if (gpu[i])
      vkDestroyBuffer(device, gpu[i], NULL);
    if (gpu_memory[i])
      vkFreeMemory(device, gpu_memory[i], NULL);
    gpu[i] = staging[i] = VK_NULL_HANDLE;
    gpu_memory[i] = staging_memory[i] = VK_NULL_HANDLE;
    mapped[i] = NULL;
  }
}

static bool create_storage_set(
    APP *app, VkDeviceSize size,
    VkBuffer gpu[MAX_FRAMES_IN_FLIGHT],
    VkDeviceMemory gpu_memory[MAX_FRAMES_IN_FLIGHT],
    VkBuffer staging[MAX_FRAMES_IN_FLIGHT],
    VkDeviceMemory staging_memory[MAX_FRAMES_IN_FLIGHT],
    void *mapped[MAX_FRAMES_IN_FLIGHT]) {
  VkDevice device = VK_CTX(app)->logical_device;
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (!create_buffer(VK_CTX(app)->physical_device, device, size,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &gpu[i],
                  &gpu_memory[i])) goto fail;
    if (!create_buffer(VK_CTX(app)->physical_device, device, size,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  &staging[i], &staging_memory[i])) goto fail;
    if (vkMapMemory(device, staging_memory[i], 0, size, 0, &mapped[i]) !=
        VK_SUCCESS) {
      LOG_ERROR("vertex-buffer staging map failed for frame %u", i);
      mapped[i] = NULL;
      goto fail;
    }
  }
  return true;
fail:
  destroy_storage_set(device, gpu, gpu_memory, staging, staging_memory, mapped);
  return false;
}

static bool wait_until_storage_is_unused(APP *app, VertexBuffer *vb) {
  VkDevice device = VK_CTX(app)->logical_device;
  VkFence render_fences[MAX_FRAMES_IN_FLIGHT];
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    render_fences[i] =
        VK_CTX(app)->render_context.frames[i].in_flight_fence;
  if (!vulkan_check(app, vkWaitForFences(device, MAX_FRAMES_IN_FLIGHT, render_fences, VK_TRUE,
                      UINT64_MAX), "vertex growth frame wait")) {
    LOG_ERROR("%s vertex-buffer growth render-fence wait failed",
              vb->debug_name ? vb->debug_name : "unnamed");
    return false;
  }
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    FrameTransferState *transfer = &VK_CTX(app)->render_context.transfers[i];
    if (transfer->submitted && !transfer->covered_by_render_fence &&
        !vulkan_check(app, vkWaitForFences(device, 1, &transfer->completion_fence, VK_TRUE,
                        UINT64_MAX), "vertex growth upload wait")) {
      LOG_ERROR("%s vertex-buffer growth orphaned-upload wait failed",
                vb->debug_name ? vb->debug_name : "unnamed");
      return false;
    }
  }
  return true;
}

static bool grow_vertex_buffer(APP *app, VertexBuffer *vb,
                               uint32_t required_capacity) {
  if (required_capacity <= vb->capacity)
    return true;
  if (required_capacity > vb->maximum_capacity) {
    LOG_ERROR("%s vertex-buffer hard capacity exceeded: required=%u max=%u",
              vb->debug_name ? vb->debug_name : "unnamed",
              required_capacity, vb->maximum_capacity);
    return false;
  }
  uint32_t new_capacity = vb->capacity ? vb->capacity : 1u;
  while (new_capacity < required_capacity) {
    uint64_t grown = (uint64_t)new_capacity + new_capacity / 2u;
    if (grown > vb->maximum_capacity) {
      new_capacity = vb->maximum_capacity;
      break;
    }
    if (grown <= new_capacity || grown > UINT32_MAX) {
      new_capacity = required_capacity;
      break;
    }
    new_capacity = (uint32_t)grown;
  }
  VkDeviceSize new_size = 0;
  if (!vertex_capacity_bytes(new_capacity, &new_size)) {
    LOG_ERROR("%s vertex-buffer capacity overflow: required=%u",
              vb->debug_name ? vb->debug_name : "unnamed",
              required_capacity);
    return false;
  }

  VkBuffer new_gpu[MAX_FRAMES_IN_FLIGHT] = {0};
  VkDeviceMemory new_gpu_memory[MAX_FRAMES_IN_FLIGHT] = {0};
  VkBuffer new_staging[MAX_FRAMES_IN_FLIGHT] = {0};
  VkDeviceMemory new_staging_memory[MAX_FRAMES_IN_FLIGHT] = {0};
  void *new_mapped[MAX_FRAMES_IN_FLIGHT] = {0};
  if (!create_storage_set(app, new_size, new_gpu,
                          new_gpu_memory,
                          new_staging, new_staging_memory, new_mapped))
    return false;

  if (!wait_until_storage_is_unused(app, vb)) {
    destroy_storage_set(VK_CTX(app)->logical_device, new_gpu, new_gpu_memory,
                        new_staging, new_staging_memory, new_mapped);
    return false;
  }

  destroy_storage_set(VK_CTX(app)->logical_device, vb->gpu_buffer,
                      vb->gpu_buffer_memory, vb->staging_buffer,
                      vb->staging_buffer_memory, vb->mapped);
  memcpy(vb->gpu_buffer, new_gpu, sizeof(new_gpu));
  memcpy(vb->gpu_buffer_memory, new_gpu_memory, sizeof(new_gpu_memory));
  memcpy(vb->staging_buffer, new_staging, sizeof(new_staging));
  memcpy(vb->staging_buffer_memory, new_staging_memory,
         sizeof(new_staging_memory));
  memcpy(vb->mapped, new_mapped, sizeof(new_mapped));
  memset(vb->upload_requested, 0, sizeof(vb->upload_requested));
  memset(vb->upload_size, 0, sizeof(vb->upload_size));
  memset(vb->upload_range_count, 0, sizeof(vb->upload_range_count));
  uint32_t old_capacity = vb->capacity;
  vb->capacity = new_capacity;
  vb->size = new_size;

  LOG_INFO("[%s] vertex buffer grow: %u -> %u vertices, required=%u, "
           "bytes=%" PRIu64,
           vb->debug_name ? vb->debug_name : "vertex-buffer", old_capacity,
           new_capacity, required_capacity, (uint64_t)new_size);
  return true;
}

bool vertex_buffer_record_upload(VertexBuffer *vb, uint32_t frame,
                                 VkCommandBuffer command_buffer,
                                 VkBufferMemoryBarrier *out_barrier) {
  if (!vb || frame >= MAX_FRAMES_IN_FLIGHT || !vb->upload_requested[frame])
    return false;
  VkDeviceSize size = vb->upload_size[frame];
  if (command_buffer == VK_NULL_HANDLE || !out_barrier || size == 0 ||
      size > vb->size) {
    LOG_ERROR("invalid %s vertex-buffer copy: frame=%u bytes=%" PRIu64
              " capacity=%" PRIu64,
              vb && vb->debug_name ? vb->debug_name : "unnamed", frame,
              (uint64_t)size, vb ? (uint64_t)vb->size : 0u);
    return false;
  }
  assert(size <= vb->size);
  uint32_t range_count = vb->upload_range_count[frame];
  if (range_count == 0 || range_count > 2)
    return false;
  VkBufferCopy copy_regions[2];
  VkDeviceSize min_offset = VK_WHOLE_SIZE;
  VkDeviceSize max_end = 0;
  for (uint32_t i = 0; i < range_count; ++i) {
    VkDeviceSize offset = vb->upload_ranges[frame][i].offset;
    VkDeviceSize range_size = vb->upload_ranges[frame][i].size;
    copy_regions[i] = (VkBufferCopy){
        .srcOffset = offset, .dstOffset = offset, .size = range_size};
    if (offset < min_offset)
      min_offset = offset;
    if (offset + range_size > max_end)
      max_end = offset + range_size;
  }
  vkCmdCopyBuffer(command_buffer, vb->staging_buffer[frame],
                  vb->gpu_buffer[frame], range_count, copy_regions);
  *out_barrier = (VkBufferMemoryBarrier){
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = vb->gpu_buffer[frame],
      .offset = min_offset,
      .size = max_end - min_offset,
  };
  return true;
}

bool create_vertex_buffer_ext_limited(APP *app, VertexBuffer *vb,
                                      uint32_t initial_vertex_capacity,
                                      uint32_t maximum_vertex_capacity,
                                      const char *debug_name) {
  memset(vb, 0, sizeof(*vb));
  vb->debug_name = debug_name;
  if (maximum_vertex_capacity == 0 ||
      initial_vertex_capacity > maximum_vertex_capacity) {
    LOG_ERROR("invalid %s vertex-buffer capacity range: initial=%u max=%u",
              debug_name ? debug_name : "unnamed", initial_vertex_capacity,
              maximum_vertex_capacity);
    return false;
  }
  if (!vertex_capacity_bytes(initial_vertex_capacity, &vb->size)) {
    LOG_ERROR("invalid initial %s vertex-buffer capacity: %u",
              debug_name ? debug_name : "unnamed", initial_vertex_capacity);
    return false;
  }
  vb->capacity = initial_vertex_capacity;
  vb->maximum_capacity = maximum_vertex_capacity;

  if (!create_storage_set(app, vb->size, vb->gpu_buffer,
                          vb->gpu_buffer_memory, vb->staging_buffer,
                          vb->staging_buffer_memory, vb->mapped))
    return false;

  // ESSENZIALE: un buffer GPU PER OGNI frame in volo, non uno solo
  // condiviso — prima, una copia nuova (per il frame N) poteva
  // sovrascrivere dati che il disegno del frame N-1 stava ancora
  // leggendo sulla GPU, confermato dalla validazione di
  // sincronizzazione (WRITE_AFTER_READ hazard). Stesso principio già
  // applicato a immagine offscreen, profondità, descriptor set. La copia è
  // registrata nello stesso command buffer del draw: la fence di render dello
  // slot protegge staging, device buffer e command buffer insieme.
  return true;
}

bool create_vertex_buffer_ext(APP *app, VertexBuffer *vb,
                              uint32_t initial_vertex_capacity,
                              const char *debug_name) {
  return create_vertex_buffer_ext_limited(app, vb, initial_vertex_capacity,
                                   UINT32_MAX, debug_name);
}

bool vertex_buffer_prepare(APP *app, VertexBuffer *vb, uint32_t count) {
  if (!app || !vb)
    return false;
  uint32_t frame =
      VK_CTX(app)->render_context.current_frame % MAX_FRAMES_IN_FLIGHT;
  if (count == 0) {
    vb->vertex_count = 0;
    vb->upload_requested[frame] = false;
    vb->upload_size[frame] = 0;
    vb->upload_range_count[frame] = 0;
    return true;
  }
  if (count > vb->capacity && !grow_vertex_buffer(app, vb, count)) {
    LOG_ERROR("%s vertex-buffer upload discarded: required=%u capacity=%u",
              vb->debug_name ? vb->debug_name : "unnamed", count,
              vb->capacity);
    vb->vertex_count = 0;
    return false;
  }
  if (count > vb->capacity) {
    vb->vertex_count = 0;
    return false;
  }
  vb->vertex_count = count;
  return true;
}

bool vertex_buffer_stage_range(APP *app, VertexBuffer *vb,
                               const Vertex *data, uint32_t first_vertex,
                               uint32_t count) {
  if (!app || !vb || !data || count == 0)
    return count == 0;
  uint32_t frame =
      VK_CTX(app)->render_context.current_frame % MAX_FRAMES_IN_FLIGHT;
  uint64_t end_vertex = (uint64_t)first_vertex + count;
  if (end_vertex > vb->vertex_count || end_vertex > vb->capacity ||
      !vb->mapped[frame]) {
    LOG_ERROR("unsafe %s vertex-buffer range rejected: first=%u count=%u "
              "vertices=%u capacity=%u",
              vb->debug_name ? vb->debug_name : "unnamed", first_vertex,
              count, vb->vertex_count, vb->capacity);
    return false;
  }
  VkDeviceSize copy_size = 0;
  if (!vertex_capacity_bytes(count, &copy_size))
    return false;
  VkDeviceSize copy_offset = (VkDeviceSize)first_vertex * sizeof(Vertex);
  if (copy_offset > vb->size || copy_size > vb->size - copy_offset)
    return false;

  memcpy((char *)vb->mapped[frame] + copy_offset, data, (size_t)copy_size);

  uint32_t range_count = vb->upload_range_count[frame];
  if (range_count > 0) {
    VkDeviceSize previous_end =
        vb->upload_ranges[frame][range_count - 1].offset +
        vb->upload_ranges[frame][range_count - 1].size;
    if (previous_end == copy_offset) {
      vb->upload_ranges[frame][range_count - 1].size += copy_size;
      vb->upload_size[frame] += copy_size;
      vb->upload_requested[frame] = true;
      return true;
    }
  }
  if (range_count >= 2) {
    LOG_ERROR("too many %s vertex-buffer upload ranges for frame %u",
              vb->debug_name ? vb->debug_name : "unnamed", frame);
    return false;
  }
  vb->upload_ranges[frame][range_count].offset = copy_offset;
  vb->upload_ranges[frame][range_count].size = copy_size;
  vb->upload_range_count[frame] = range_count + 1;
  vb->upload_size[frame] += copy_size;
  vb->upload_requested[frame] = true;
  return true;
}

void update_vertex_buffer(APP *app, VertexBuffer *vb, const Vertex *data,
                          uint32_t count) {
  if (!vb)
    return;
  if (count == 0 || !data) {
    (void)vertex_buffer_prepare(app, vb, 0);
    return;
  }
  if (!vertex_buffer_prepare(app, vb, count) ||
      !vertex_buffer_stage_range(app, vb, data, 0, count))
    vb->vertex_count = 0;
}

void vertex_buffer_begin_frame(VertexBuffer *vb, uint32_t frame) {
  if (vb && frame < MAX_FRAMES_IN_FLIGHT) {
    vb->upload_requested[frame] = false;
    vb->upload_size[frame] = 0;
    vb->upload_range_count[frame] = 0;
  }
}

void destroy_vertex_buffer_ext(APP *app, VertexBuffer *vb) {
  VkDevice device = VK_CTX(app)->logical_device;
  if (!device)
    return;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vb->mapped[i])
      vkUnmapMemory(device, vb->staging_buffer_memory[i]);
    if (vb->staging_buffer[i])
      vkDestroyBuffer(device, vb->staging_buffer[i], NULL);
    if (vb->staging_buffer_memory[i])
      vkFreeMemory(device, vb->staging_buffer_memory[i], NULL);
    if (vb->gpu_buffer[i])
      vkDestroyBuffer(device, vb->gpu_buffer[i], NULL);
    if (vb->gpu_buffer_memory[i])
      vkFreeMemory(device, vb->gpu_buffer_memory[i], NULL);
  }
  memset(vb, 0, sizeof(*vb));
}
