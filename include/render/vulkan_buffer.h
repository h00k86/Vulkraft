#pragma once
#include "render/gfx_vulkan.h" /* PORTING E14: tipi del backend */

#include <vulkan/vulkan.h>
typedef struct APP APP;


VkCommandBuffer begin_single_time_commands(APP *app);

void end_single_time_commands(APP *app, VkCommandBuffer cmd);
uint32_t find_memory_type(VkPhysicalDevice phys,
                          uint32_t typeFilter,
                          VkMemoryPropertyFlags props) ;


void create_buffer(VkPhysicalDevice phys, VkDevice device, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer* buffer, VkDeviceMemory* bufferMemory);
