/* Real decode failure and destructor; Vulkan calls are intercepted headlessly. */
#include "../src/render/texture_manager.c"
#include <assert.h>
static int descriptor_writes, sampler_destroys, view_destroys;
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkDestroySampler(VkDevice d, VkSampler s, const VkAllocationCallbacks *a) {
  ++sampler_destroys;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice d, VkImageView v, const VkAllocationCallbacks *a) {
  ++view_destroys;
}
VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(VkDevice d, uint32_t n, const VkWriteDescriptorSet *writes,
                                                  uint32_t c, const VkCopyDescriptorSet *copies) {
  descriptor_writes += (int)n;
}
int main(void) {
  APP app = {0}; VulkanContext context = {0}; RendererGpu gpu = {0};
  app.context = &context; app.renderer.gpu = &gpu;
  g_slot_used[1] = true;
  /* Existing non-image file ensures mtime changes but image decoding fails. */
  strcpy(g_paths[1], "tests/texture_reload_failure_test.c");
  g_textures[1] = (Texture){.sampler = (VkSampler)(uintptr_t)1,
                           .view = (VkImageView)(uintptr_t)2,
                           .width = 16, .height = 16, .mipLevels = 1,
                           .address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  assert(!texture_manager_check_hot_reload(&app, 1));
  assert(texture_manager_is_valid(1) && g_lastMtime[1] == 0);
  assert(g_textures[1].view == (VkImageView)(uintptr_t)2 && g_textures[1].width == 16);
  assert(descriptor_writes == 0 && sampler_destroys == 0 && view_destroys == 0);
  destroy_texture(&app, &g_textures[1]);
  assert(sampler_destroys == 1 && view_destroys == 1);
  assert(!g_textures[1].image && !g_textures[1].memory && !g_textures[1].view &&
         !g_textures[1].sampler && g_textures[1].width == 0 && g_textures[1].mipLevels == 0);
  destroy_texture(&app, &g_textures[1]);
  assert(sampler_destroys == 1 && view_destroys == 1);
  puts("Real texture decode failure and destructor: PASS");
  return 0;
}
