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

void init_vulkan(APP *app) {
  app->window = create_window(app);
  if (!app->window) { LOG_ERROR("Window initialization failed"); exit(EXIT_FAILURE); }
  VK_CTX(app)->instance = create_instance();
  VK_CTX(app)->render_context.surface = create_surface(app);
  VK_CTX(app)->physical_device = select_device(VK_CTX(app)->instance, VK_CTX(app)->render_context.surface);
  VK_CTX(app)->logical_device = create_logical_device(app);
  VK_CTX(app)->render_context.swapchain = create_swapchain(app);
  create_image_views(app);
  create_renderpass(app);
  create_descriptor_set_layout(app);
  create_pipeline(app);
  create_command_buffers(app);
  for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f)
    create_ubo_camera(app, &app->renderer.gpu->camera_ubo[f]);
  char path[APP_PATH_MAX];
  paths_engine_file(path, sizeof(path), "assets/fonts/font2.png");
  load_texture(app, path, &app->renderer.gpu->texture);
  create_descriptor_pool(app);
  create_hud_descriptor_set(app);
  create_sync_objects(app);
  create_framebuffers(app);
  create_vertex_buffer_ext(app, &app->renderer.gpu->hud_buffer, HUD_INITIAL_VERTEX_CAPACITY, "UI");
}

void destroy_vulkan(APP *app) {
  VkDevice device = VK_CTX(app)->logical_device;
  if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
  frame_dump_shutdown(app);
  destroy_framebuffers(app);
  texture_manager_shutdown(app);
  destroy_pipeline(app);
  destroy_descriptors(app);
  for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f)
    destroy_ubo_camera(app, &app->renderer.gpu->camera_ubo[f]);
  destroy_vertex_buffer_ext(app, &app->renderer.gpu->hud_buffer);
  text_renderer_shutdown();
  destroy_texture(app, &app->renderer.gpu->texture);
  destroy_renderpass(app);
  destroy_swapchain(app);
  destroy_command_buffers(app);
  destroy_sync_objects(app);
  destroy_surface(app);
  destroy_logical_device(app);
  destroy_instance(VK_CTX(app)->instance);
  destroy_window(app->window);
}
