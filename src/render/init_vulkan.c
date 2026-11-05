#include "render/init_vulkan.h"
#include "app/application.h"
#include "platform/window.h"
#include "platform/paths.h"
#include "render/vulkan_instance.h"
#include "render/vulkan_surface.h"
#include "render/vulkan_physical_device.h"
#include "render/vulkan_logical_device.h"
#include "render/vulkan_swapchain.h"
#include "render/vulkan_renderpass.h"
#include "render/vulkan_framebuffer.h"
#include "render/vulkan_pipeline.h"
#include "render/vulkan_descriptors.h"
#include "render/vulkan_uniform_buffer.h"
#include "render/vulkan_command.h"
#include "render/vulkan_sync.h"
#include "render/vulkan_vertex_buffer.h"
#include "render/text_renderer.h"
#include "render/texture_manager.h"
#include "render/frame_dump.h"
#include "log/log.h"
#include <stdlib.h>

// Each completed step transfers ownership to APP; shutdown accepts every prefix.
#define INIT_STEP(code_, name_, expression_) do { \
  app->error = (AppError){code_, name_}; \
  if (!(expression_)) return false; \
} while (0)

/* Ordine di dipendenza: finestra -> istanza/superficie -> device -> swapchain
 * e risorse del frame. Gli handle sono pubblicati progressivamente in APP;
 * il chiamante gfx_init esegue il rollback se uno step fallisce. */
bool init_vulkan(APP *app) {
  INIT_STEP(APP_ERROR_WINDOW, "window", (app->window = create_window(app)));
  INIT_STEP(APP_ERROR_GRAPHICS, "instance", (VK_CTX(app)->instance = create_instance()));
  INIT_STEP(APP_ERROR_GRAPHICS, "surface", (VK_CTX(app)->render_context.surface = create_surface(app)));
  INIT_STEP(APP_ERROR_GRAPHICS, "physical device", (VK_CTX(app)->physical_device = select_device(VK_CTX(app)->instance, VK_CTX(app)->render_context.surface)));
  INIT_STEP(APP_ERROR_GRAPHICS, "logical device", (VK_CTX(app)->logical_device = create_logical_device(app)));
  app->error = (AppError){APP_ERROR_GRAPHICS, "swapchain"};
  VK_CTX(app)->render_context.swapchain = create_swapchain(app);
  if (!VK_CTX(app)->render_context.swapchain.handle) return false;
  INIT_STEP(APP_ERROR_GRAPHICS, "image views", create_image_views(app));
  INIT_STEP(APP_ERROR_GRAPHICS, "render pass", create_renderpass(app));
  INIT_STEP(APP_ERROR_GRAPHICS, "descriptor layout", create_descriptor_set_layout(app));
  INIT_STEP(APP_ERROR_GRAPHICS, "pipeline", create_pipeline(app));
  INIT_STEP(APP_ERROR_GRAPHICS, "command buffers", create_command_buffers(app));
  for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f)
    INIT_STEP(APP_ERROR_GRAPHICS, "camera buffer", create_ubo_camera(app, &app->renderer.gpu->camera_ubo[f]));
  char path[APP_PATH_MAX];
  INIT_STEP(APP_ERROR_RESOURCE, "font atlas path", app_resource_path(app, APP_RESOURCE_FONT_ATLAS, path, sizeof(path)));
  load_texture(app, path, &app->renderer.gpu->texture);
  INIT_STEP(APP_ERROR_RESOURCE, "font atlas", app->renderer.gpu->texture.view && app->renderer.gpu->texture.sampler);
  INIT_STEP(APP_ERROR_GRAPHICS, "descriptor pool", create_descriptor_pool(app));
  INIT_STEP(APP_ERROR_GRAPHICS, "descriptor sets", create_hud_descriptor_set(app));
  INIT_STEP(APP_ERROR_GRAPHICS, "synchronization", create_sync_objects(app));
  INIT_STEP(APP_ERROR_GRAPHICS, "framebuffers", create_framebuffers(app));
  INIT_STEP(APP_ERROR_GRAPHICS, "vertex buffer", create_vertex_buffer_ext(app, &app->renderer.gpu->hud_buffer, HUD_INITIAL_VERTEX_CAPACITY, "UI"));
  app->error = (AppError){0};
  return true;
}
#undef INIT_STEP

/* Il cleanup tollera gli handle mancanti di un avvio parziale. Prima si ferma
 * il lavoro GPU, poi si distruggono le risorse dipendenti, infine device,
 * superficie/istanza e finestra. Non sostituire l'errore che ha causato lo stop. */
void destroy_vulkan(APP *app) {
  if (!app) return;
  if (app->context) {
    if (VK_CTX(app)->logical_device) {
      vkDeviceWaitIdle(VK_CTX(app)->logical_device);
      frame_dump_shutdown(app);
      destroy_framebuffers(app);
      if (app->renderer.gpu) {
        texture_manager_shutdown(app);
        destroy_pipeline(app);
        destroy_descriptors(app);
        for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f)
          destroy_ubo_camera(app, &app->renderer.gpu->camera_ubo[f]);
        destroy_vertex_buffer_ext(app, &app->renderer.gpu->hud_buffer);
        destroy_texture(app, &app->renderer.gpu->texture);
      }
      destroy_renderpass(app);
      destroy_swapchain(app);
      destroy_command_buffers(app);
      destroy_sync_objects(app);
      destroy_logical_device(app);
    }
    if (VK_CTX(app)->instance) {
      destroy_surface(app);
      destroy_instance(VK_CTX(app)->instance);
    }
    *app->context = (VulkanContext){0};
  }
  text_renderer_shutdown();
  destroy_window(app->window);
  app->window = NULL;
  app->initialized = false;
}
