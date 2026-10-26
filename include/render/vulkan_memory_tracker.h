#pragma once
#include "render/gfx_vulkan.h" /* PORTING E14: tipi del backend */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <vulkan/vulkan.h>

typedef enum {
  VULKAN_MEMORY_RESOURCE_BUFFER,
  VULKAN_MEMORY_RESOURCE_IMAGE,
} VulkanMemoryResourceKind;

typedef struct {
  VkDeviceSize tracked_bytes;
  VkDeviceSize device_local_bytes;
  VkDeviceSize host_visible_bytes;
  VkDeviceSize heap_bytes[VK_MAX_MEMORY_HEAPS];
  uint32_t active_allocations;
} VulkanMemorySnapshot;

void vulkan_memory_track_allocation(
    VkPhysicalDevice physical_device, VkDeviceMemory memory,
    VulkanMemoryResourceKind kind, const char *name,
    VkDeviceSize requested_bytes, const VkMemoryRequirements *requirements,
    uint32_t memory_type_index, int frame_index, bool persistent);

void vulkan_memory_track_image_info(VkDeviceMemory memory, uint32_t width,
                                    uint32_t height, VkFormat format,
                                    uint32_t mip_levels,
                                    uint32_t array_layers);
void vulkan_memory_track_label(VkDeviceMemory memory, const char *name,
                               int frame_index, bool persistent);
void vulkan_memory_track_free(VkDeviceMemory memory);
VulkanMemorySnapshot vulkan_memory_snapshot(void);
void vulkan_memory_write_inventory(FILE *stream,
                                   VkPhysicalDevice physical_device,
                                   const char *label);
