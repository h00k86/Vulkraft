#include "render/vulkan_framebuffer.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "log/log.h"
#include <stdlib.h>

void create_framebuffers(APP *app) {
  RenderContext *ctx = &VK_CTX(app)->render_context;
  Swapchain *sc = &ctx->swapchain;

  sc->framebuffers = malloc(sizeof(VkFramebuffer) * sc->image_count);
  if (!sc->framebuffers) {
    LOG_ERROR("cannot allocate swapchain framebuffers");
    exit(1);
  }

  for (uint32_t i = 0; i < sc->image_count; i++) {
    // Array di 2 attachment
    VkImageView attachments[] = {sc->image_views[i]};

    VkFramebufferCreateInfo fbInfo = {0};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = ctx->renderpass;
    fbInfo.attachmentCount = 1; // Portato a 2
    fbInfo.pAttachments = attachments;
    fbInfo.width = sc->extent.width;
    fbInfo.height = sc->extent.height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(VK_CTX(app)->logical_device, &fbInfo, NULL,
                            &sc->framebuffers[i]) != VK_SUCCESS) {
      exit(1);
    }
  }
}

void destroy_framebuffers(APP *app) {

  for (uint32_t i = 0; i < VK_CTX(app)->render_context.swapchain.image_count;
       i++) {
    vkDestroyFramebuffer(VK_CTX(app)->logical_device,
                         VK_CTX(app)->render_context.swapchain.framebuffers[i],
                         NULL);
  }

  free(VK_CTX(app)->render_context.swapchain.framebuffers);
  VK_CTX(app)->render_context.swapchain.framebuffers = NULL;
  LOG_INFO("destroied framebuffer");
}
