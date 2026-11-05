#include "log/log.h"
#include <stdlib.h>
#include "render/vulkan_uniform_buffer.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "render/vulkan_buffer.h"
#include <string.h>

bool create_ubo_camera(APP *app, UniformBuffer *ub) {
  VkDeviceSize bufferSize = sizeof(CameraData);

  if (!create_buffer(VK_CTX(app)->physical_device, VK_CTX(app)->logical_device, bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &ub->buffer, &ub->memory)) return false;

  if (vkMapMemory(VK_CTX(app)->logical_device, ub->memory, 0, bufferSize, 0,
                  &ub->mapped) != VK_SUCCESS) {
    LOG_ERROR("camera UBO map failed");
    ub->mapped = NULL;
    return false;
  }
  return true;
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
    vkFreeMemory(device, ub->memory, NULL);
  }
  *ub = (UniformBuffer){0};
}
