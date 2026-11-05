#include "render/vulkan_shader.h"
#include "render/gfx_vulkan.h"

VkShaderModule load_shader(VkDevice device, const char *path) {
  if (!path || !*path) return VK_NULL_HANDLE;
  /* The caller resolves application resources explicitly. */

  FILE *f = fopen(path, "rb");
  if (!f) {
    LOG_ERROR("shader %s not found", path);
    return VK_NULL_HANDLE;
  }

  long size = -1;
  if (fseek(f, 0, SEEK_END) == 0) size = ftell(f);
  if (size <= 0 || size % 4 != 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return VK_NULL_HANDLE; }

  uint32_t *code = malloc(size);
  if (!code) {
    fclose(f);
    LOG_ERROR("malloc failed for shader");
    return VK_NULL_HANDLE;
  }

  if (fread(code, 1, (size_t)size, f) != (size_t)size) {
    fclose(f);
    free(code);
    LOG_ERROR("failed reading shader");
    return VK_NULL_HANDLE;
  }

  fclose(f);

  VkShaderModuleCreateInfo cInfo = {0};
  cInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  cInfo.codeSize = size;
  cInfo.pCode = code;

  VkShaderModule module;

  if (vkCreateShaderModule(device, &cInfo, NULL, &module) != VK_SUCCESS) {
    LOG_ERROR("failed to create shader module");
    free(code);
    return VK_NULL_HANDLE;
  }

  free(code);
  return module;
}
