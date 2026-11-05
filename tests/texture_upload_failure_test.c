/* Real upload path, with a map failure returning an unusable pointer. */
#include "../src/render/vulkan_texture.c"
#include <assert.h>
static int pixels_live, buffers_live, memory_live;
unsigned char *stbi_load(const char *f,int *w,int *h,int *c,int requested) {
  *w=*h=1; *c=4; ++pixels_live; return calloc(4,1);
}
void stbi_image_free(void *p) { --pixels_live; free(p); }
bool create_buffer(VkPhysicalDevice p,VkDevice d,VkDeviceSize s,VkBufferUsageFlags u,VkMemoryPropertyFlags flags,VkBuffer *b,VkDeviceMemory *m) {
  *b=(VkBuffer)(uintptr_t)1; *m=(VkDeviceMemory)(uintptr_t)2;
  ++buffers_live; ++memory_live; return true;
}
VkResult vkMapMemory(VkDevice d,VkDeviceMemory m,VkDeviceSize o,VkDeviceSize s,VkMemoryMapFlags f,void **p) {
  *p=(void *)1; return VK_ERROR_MEMORY_MAP_FAILED;
}
void vkDestroyBuffer(VkDevice d,VkBuffer b,const VkAllocationCallbacks *a) { --buffers_live; }
void vkFreeMemory(VkDevice d,VkDeviceMemory m,const VkAllocationCallbacks *a) { --memory_live; }
VkResult vkCreateSampler(VkDevice d,const VkSamplerCreateInfo *i,const VkAllocationCallbacks *a,VkSampler *s) {
  *s=(VkSampler)(uintptr_t)99; return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}
VkCommandBuffer begin_single_time_commands(APP *a) { assert(0); return VK_NULL_HANDLE; }
bool end_single_time_commands(APP *a,VkCommandBuffer c) { assert(0); return false; }
uint32_t find_memory_type(VkPhysicalDevice p,uint32_t filter,VkMemoryPropertyFlags flags) { assert(0); return UINT32_MAX; }
void vulkan_memory_track_free(VkDeviceMemory m) {}
void vulkan_memory_track_allocation(VkPhysicalDevice p,VkDeviceMemory m,VulkanMemoryResourceKind k,const char *n,VkDeviceSize s,const VkMemoryRequirements *r,uint32_t t,int f,bool persistent) { assert(0); }
void vulkan_memory_track_image_info(VkDeviceMemory m,uint32_t w,uint32_t h,VkFormat f,uint32_t levels,uint32_t layers) { assert(0); }
int main(void) {
  VulkanContext context={0}; APP app={.context=&context}; Texture t={0};
  load_texture(&app,"mock-pixel",&t);
  assert(!pixels_live && !buffers_live && !memory_live);
  assert(!t.image && !t.memory && !t.view && !t.sampler && !t.width);
  t.mipLevels=1;
  assert(!create_sampler(&app,&t,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE));
  assert(!t.sampler);
  puts("Texture map/sampler failures and staging cleanup: PASS");
}
