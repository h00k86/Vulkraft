#include "log/log.h"
#include <stdlib.h>
#include "render/vulkan_uniform_buffer.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "render/vulkan_buffer.h"
#include "render/vulkan_memory_tracker.h"
#include <string.h>

void create_ubo_camera(APP *app, UniformBuffer *ub) {
  VkDeviceSize bufferSize = sizeof(CameraData);
  ub->size = bufferSize;

  create_buffer(VK_CTX(app)->physical_device, VK_CTX(app)->logical_device, bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &ub->buffer, &ub->memory);

  if (vkMapMemory(VK_CTX(app)->logical_device, ub->memory, 0, bufferSize, 0,
                  &ub->mapped) != VK_SUCCESS) {
    LOG_ERROR("camera UBO map failed");
    exit(1);
  }
}

void update_ubo_camera(UniformBuffer *ub, const CameraData *data) {
  if (ub->mapped) {
    memcpy(ub->mapped, data, sizeof(CameraData));
  }
}

void destroy_ubo_camera(APP *app, UniformBuffer *ub) {
  VkDevice device = VK_CTX(app)->logical_device;
  if (ub->mapped) {
    vkUnmapMemory(device, ub->memory);
  }
  // FIX: Cambiato da ub->handle a ub->buffer
  if (ub->buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, ub->buffer, NULL);
  }
  if (ub->memory != VK_NULL_HANDLE) {
    vulkan_memory_track_free(ub->memory);
    vkFreeMemory(device, ub->memory, NULL);
  }
}
