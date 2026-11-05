#pragma once
#include "render/gfx_vulkan.h"
#include "log/log.h"

/* Preserve the first fatal error through shutdown. Vulkan stays below gfx. */
static inline bool vulkan_check(APP *app, VkResult result, const char *stage) {
  if (result == VK_SUCCESS) return true;
  if (app && app->error.code == APP_ERROR_NONE) {
    app->error = (AppError){APP_ERROR_GRAPHICS, stage};
    LOG_ERROR("%s failed (VkResult %d)", stage, (int)result);
  }
  return false;
}
static inline bool vulkan_frame_ready(APP *app) {
  if (!app || app->error.code) return false;
  if (!app->context || !app->context->logical_device || !app->renderer.gpu ||
      app->context->render_context.current_frame >= MAX_FRAMES_IN_FLIGHT) {
    app->error = (AppError){APP_ERROR_STATE, "render frame without valid backend"};
    return false;
  }
  return true;
}
