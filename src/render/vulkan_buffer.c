#include "render/vulkan_buffer.h"
#include "render/gfx_vulkan.h"
#include "log/log.h"
#include "render/vulkan_memory_tracker.h"
#include <stdlib.h>
#include <string.h>

uint32_t find_memory_type(VkPhysicalDevice phys, uint32_t type_filter,
                          VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(phys, &mem_props);

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  fprintf(stderr,
          "[FATAL] find_memory_type failed\n"
          "type_filter = %u\n",
          type_filter);

  abort(); // IMPORTANTISSIMO: mai continuare
}

void create_buffer(VkPhysicalDevice phys, VkDevice device, VkDeviceSize size,
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                   VkBuffer *buffer, VkDeviceMemory *bufferMemory) {

  VkBufferCreateInfo bufferInfo = {.sType =
                                       VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                   .size = size,
                                   .usage = usage,
                                   .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

  if (vkCreateBuffer(device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
    fprintf(stderr, "Errore creazione buffer\n");
    exit(1);
  }

  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(device, *buffer, &memReqs);

  uint32_t memoryTypeIndex =
      find_memory_type(phys, memReqs.memoryTypeBits, properties);
  VkMemoryAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      .memoryTypeIndex = memoryTypeIndex};

  if (vkAllocateMemory(device, &allocInfo, NULL, bufferMemory) != VK_SUCCESS) {
    fprintf(stderr, "Errore allocazione memoria buffer\n");
    exit(1);
  }

  vulkan_memory_track_allocation(
      phys, *bufferMemory, VULKAN_MEMORY_RESOURCE_BUFFER,
      "unclassified-buffer", size, &memReqs, memoryTypeIndex, -1, true);

  vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

VkCommandBuffer begin_single_time_commands(APP *app) {
  VkCommandBufferAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandPool = VK_CTX(app)->render_context.commands.pool,
      .commandBufferCount = 1};

  VkCommandBuffer commandBuffer;

  vkAllocateCommandBuffers(VK_CTX(app)->logical_device, &allocInfo,
                           &commandBuffer);

  VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void end_single_time_commands(APP *app, VkCommandBuffer commandBuffer) {

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .commandBufferCount = 1,
                             .pCommandBuffers = &commandBuffer};

  vkQueueSubmit(VK_CTX(app)->graphic_queue, 1, &submitInfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(VK_CTX(app)->graphic_queue);

  vkFreeCommandBuffers(VK_CTX(app)->logical_device,
                       VK_CTX(app)->render_context.commands.pool, 1,
                       &commandBuffer);
}
