/* Check transactional replacement and retry with deterministic GPU adapters. */
#include "../src/render/texture_manager.c"
#include <assert.h>

static long long file_mtime = 2;
static int loader_result, loads, tiled_loads, waits, writes, old_destroyed, partial_destroyed;
static Texture old_texture;
static const VkImageView new_view = (VkImageView)(uintptr_t)4;

bool platform_file_mtime(const char *path, long long *out) {
  *out = file_mtime;
  return true;
}
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) {
  ++waits;
  return VK_SUCCESS;
}
void load_texture(APP *app, const char *path, Texture *texture) {
  ++loads;
  if (loader_result >= 1) texture->view = new_view;
  if (loader_result == 2) {
    texture->sampler = (VkSampler)(uintptr_t)3;
    texture->width = texture->height = 16;
    texture->mipLevels = 1;
    texture->address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
}
void load_texture_tiled(APP *app, const char *path, Texture *texture) {
  ++tiled_loads;
  load_texture(app, path, texture);
  texture->address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  if (loader_result == 2) texture->mipLevels = 5;
}
void destroy_texture(APP *app, Texture *texture) {
  if (texture->view == old_texture.view) {
    assert(waits == 1 && writes == 1); /* replacement published before retirement */
    ++old_destroyed;
  } else if (texture->view == new_view) {
    ++partial_destroyed;
  }
  *texture = (Texture){0};
}
void update_descriptor_texture_slot_no_wait(APP *app, int slot, Texture *texture) {
  assert(slot == 1 && texture->view == new_view && waits == 1);
  ++writes;
}

static void check_mode(VkSamplerAddressMode mode) {
  APP app = {0}; VulkanContext context = {0}; app.context = &context;
  loads = tiled_loads = waits = writes = old_destroyed = partial_destroyed = 0;
  old_texture = (Texture){.view = (VkImageView)(uintptr_t)2,
                         .sampler = (VkSampler)(uintptr_t)1,
                         .width = 8, .height = 8, .mipLevels = 1, .address_mode = mode};
  g_textures[1] = old_texture; g_slot_used[1] = true; g_lastMtime[1] = 1;
  strcpy(g_paths[1], "image.png");
  for (loader_result = 0; loader_result < 2; ++loader_result) {
    assert(!texture_manager_check_hot_reload(&app, 1));
    assert(g_textures[1].view == old_texture.view && g_textures[1].sampler == old_texture.sampler);
    assert(g_textures[1].width == 8 && g_slot_used[1] && g_lastMtime[1] == 1);
    assert(writes == 0 && old_destroyed == 0);
  }
  assert(partial_destroyed == 1);
  /* Retry at the very same mtime succeeds once the file is complete. */
  assert(texture_manager_check_hot_reload(&app, 1));
  assert(loads == 3 && waits == 1 && writes == 1 && old_destroyed == 1);
  assert(g_lastMtime[1] == file_mtime && g_slot_used[1]);
  assert(g_textures[1].view == new_view && g_textures[1].address_mode == mode);
  assert(g_textures[1].mipLevels == (mode == VK_SAMPLER_ADDRESS_MODE_REPEAT ? 5u : 1u));
  assert(tiled_loads == (mode == VK_SAMPLER_ADDRESS_MODE_REPEAT ? 3 : 0));
  assert(!texture_manager_check_hot_reload(&app, 1));
  assert(loads == 3 && writes == 1 && old_destroyed == 1);
}
int main(void) {
  check_mode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
  check_mode(VK_SAMPLER_ADDRESS_MODE_REPEAT);
  puts("Texture reload transaction regressions: PASS");
  return 0;
}
