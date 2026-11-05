#include "render/vulkan_buffer.h"
#include "render/gfx_vulkan.h"
#include "render/vulkan_memory_tracker.h"
#include <stdint.h>

uint32_t find_memory_type(VkPhysicalDevice phys, uint32_t type_filter,
                          VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties available;
  vkGetPhysicalDeviceMemoryProperties(phys, &available);
  for (uint32_t i = 0; i < available.memoryTypeCount; ++i)
    if ((type_filter & (1u << i)) &&
        (available.memoryTypes[i].propertyFlags & properties) == properties) return i;
  return UINT32_MAX;
}

bool create_buffer(VkPhysicalDevice phys, VkDevice device, VkDeviceSize size,
                    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                    VkBuffer *buffer, VkDeviceMemory *memory) {
  *buffer = VK_NULL_HANDLE; *memory = VK_NULL_HANDLE;
  VkBufferCreateInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                             .size = size, .usage = usage,
                             .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
  if (!size || vkCreateBuffer(device, &info, NULL, buffer) != VK_SUCCESS) {
    *buffer = VK_NULL_HANDLE; return false;
  }
  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(device, *buffer, &requirements);
  uint32_t type = find_memory_type(phys, requirements.memoryTypeBits, properties);
  VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                     .allocationSize = requirements.size, .memoryTypeIndex = type};
  if (type == UINT32_MAX || vkAllocateMemory(device, &allocation, NULL, memory) != VK_SUCCESS) {
    *memory = VK_NULL_HANDLE;
    vkDestroyBuffer(device, *buffer, NULL); *buffer = VK_NULL_HANDLE; return false;
  }
  if (vkBindBufferMemory(device, *buffer, *memory, 0) != VK_SUCCESS) {
    vkDestroyBuffer(device, *buffer, NULL); vkFreeMemory(device, *memory, NULL);
    *buffer = VK_NULL_HANDLE; *memory = VK_NULL_HANDLE; return false;
  }
  vulkan_memory_track_allocation(phys, *memory, VULKAN_MEMORY_RESOURCE_BUFFER,
                                 "unclassified-buffer", size, &requirements, type, -1, true);
  return true;
}

VkCommandBuffer begin_single_time_commands(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  VkCommandBufferAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandPool = VK_CTX(app)->render_context.commands.pool,
      .commandBufferCount = 1};
  VkCommandBuffer command = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(device, &allocation, &command) != VK_SUCCESS) return VK_NULL_HANDLE;
  VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                   .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  if (vkBeginCommandBuffer(command, &begin) != VK_SUCCESS) {
    vkFreeCommandBuffers(device, allocation.commandPool, 1, &command); return VK_NULL_HANDLE;
  }
  return command;
}

bool end_single_time_commands(APP *app, VkCommandBuffer command) {
  VkDevice device = VK_CTX(app)->logical_device;
  bool ok = vkEndCommandBuffer(command) == VK_SUCCESS;
  if (ok) {
    VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                           .commandBufferCount = 1, .pCommandBuffers = &command};
    ok = vkQueueSubmit(VK_CTX(app)->graphic_queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS;
    if (ok) ok = vkQueueWaitIdle(VK_CTX(app)->graphic_queue) == VK_SUCCESS;
  }
  vkFreeCommandBuffers(device, VK_CTX(app)->render_context.commands.pool, 1, &command);
  return ok;
}
