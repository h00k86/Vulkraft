#include "../src/render/vulkan_buffer.c"
#include "../src/render/vulkan_uniform_buffer.c"
#include <assert.h>
static int failure, buffers, memories, tracked;
VkResult vkCreateBuffer(VkDevice d,const VkBufferCreateInfo *i,const VkAllocationCallbacks *a,VkBuffer *b) {
  *b=(VkBuffer)(uintptr_t)99;
  if(failure==1) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  ++buffers; return VK_SUCCESS;
}
void vkDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks *a) { if(b) { assert(buffers==1); --buffers; } }
void vkGetBufferMemoryRequirements(VkDevice d,VkBuffer b,VkMemoryRequirements *r) { *r=(VkMemoryRequirements){.size=256,.memoryTypeBits=1}; }
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice p,VkPhysicalDeviceMemoryProperties *m) {
  *m=(VkPhysicalDeviceMemoryProperties){.memoryTypeCount=1};
  m->memoryTypes[0].propertyFlags=failure==2?0:VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}
VkResult vkAllocateMemory(VkDevice d,const VkMemoryAllocateInfo *i,const VkAllocationCallbacks *a,VkDeviceMemory *m) {
  *m=(VkDeviceMemory)(uintptr_t)88;
  if(failure==3) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
  ++memories; return VK_SUCCESS;
}
void vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks *a) { if(m) { assert(memories==1); --memories; } }
VkResult vkBindBufferMemory(VkDevice d,VkBuffer b,VkDeviceMemory m,VkDeviceSize o) { return failure==4?VK_ERROR_OUT_OF_DEVICE_MEMORY:VK_SUCCESS; }
VkResult vkMapMemory(VkDevice d,VkDeviceMemory m,VkDeviceSize o,VkDeviceSize s,VkMemoryMapFlags f,void **p) {
  *p=(void *)1; return failure==5?VK_ERROR_MEMORY_MAP_FAILED:VK_SUCCESS;
}
void vkUnmapMemory(VkDevice d,VkDeviceMemory m) {}
void vulkan_memory_track_allocation(VkPhysicalDevice p,VkDeviceMemory m,VulkanMemoryResourceKind k,const char *n,VkDeviceSize s,const VkMemoryRequirements *r,uint32_t t,int f,bool persistent) { ++tracked; }
void vulkan_memory_track_free(VkDeviceMemory m) { --tracked; }
int main(void) {
  VulkanContext context={0}; APP app={.context=&context};
  for(failure=1;failure<=6;++failure) {
    UniformBuffer u={0};
    assert(create_ubo_camera(&app,&u)==(failure==6));
    destroy_ubo_camera(&app,&u); destroy_ubo_camera(&app,&u);
    assert(!buffers && !memories && !tracked);
  }
  puts("Buffer create/type/allocation/bind/map failures and cleanup: PASS");
}
