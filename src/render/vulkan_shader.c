#include "render/vulkan_shader.h"
#include "render/gfx_vulkan.h"
#include "platform/paths.h"

VkShaderModule load_shader(VkDevice device, const char *path) {
  /* MACROTASK MULTI-GIOCO: gli shader sono risorse del MOTORE, risolte
   * contro la root del motore (la cartella di lavoro e' quella del
   * progetto aperto). */
  char resolved[APP_PATH_MAX];
  if (path && path[0] != '/' && paths_engine_file(resolved, sizeof(resolved), path))
    path = resolved;

  FILE *f = fopen(path, "rb");
  if (!f) {
    LOG_ERROR("shader %s not found", path);
    exit(1);
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);

  uint32_t *code = malloc(size);
  if (!code) {
    fclose(f);
    LOG_ERROR("malloc failed for shader");
    exit(1);
  }

  if (fread(code, 1, (size_t)size, f) != (size_t)size) {
    fclose(f);
    free(code);
    LOG_ERROR("failed reading shader");
    exit(1);
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
    exit(1);
  }

  free(code);
  return module;
}
