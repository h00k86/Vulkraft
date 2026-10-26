#include "render/vulkan_descriptors.h"
#include "render/gfx_vulkan.h"
#include "app/application.h"
#include "log/log.h"
#include <stdlib.h>

void create_descriptor_set_layout(APP *app) {

  VkDescriptorSetLayoutBinding bindings[2] = {0}; // era [3]

  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT; // esteso al
                                                         // fragment:
                                                         // serve
                                                         // camera.time
                                                         // per
                                                         // l'increspatura
                                                         // dell'acqua
                                                         // in tri.frag

  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = MAX_TEXTURE_SLOTS;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2, // era 3
      .pBindings = bindings};

  if (vkCreateDescriptorSetLayout(VK_CTX(app)->logical_device, &info, NULL,
                                  &app->renderer.gpu->descriptor_set_layout) !=
      VK_SUCCESS) {
    LOG_ERROR("descriptor set layout failed");
    exit(1);
  }
}

void create_descriptor_pool(APP *app) {
  VkDescriptorPoolSize sizes[2] = {
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURE_SLOTS * MAX_FRAMES_IN_FLIGHT}
  };
  VkDescriptorPoolCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = 2, .pPoolSizes = sizes, .maxSets = MAX_FRAMES_IN_FLIGHT
  };
  if (vkCreateDescriptorPool(VK_CTX(app)->logical_device, &info, NULL, &app->renderer.gpu->descriptor_pool) != VK_SUCCESS) exit(1);
}

void create_hud_descriptor_set(APP *app) {
  VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
  for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f)
    layouts[f] = app->renderer.gpu->descriptor_set_layout;

  VkDescriptorSetAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = app->renderer.gpu->descriptor_pool,
      .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
      .pSetLayouts = layouts};

  if (vkAllocateDescriptorSets(VK_CTX(app)->logical_device, &alloc,
                               app->renderer.gpu->hud_descriptor_set) !=
      VK_SUCCESS) {
    LOG_ERROR("hud descriptor allocation failed");
    exit(1);
  }

  VkDescriptorImageInfo imageInfos[MAX_TEXTURE_SLOTS];
  for (int i = 0; i < MAX_TEXTURE_SLOTS; i++) {
    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfos[i].imageView = app->renderer.gpu->texture.view;
    imageInfos[i].sampler = app->renderer.gpu->texture.sampler;
  }

  for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
    VkDescriptorBufferInfo uboInfo = {
        .buffer = app->renderer.gpu->camera_ubo[f].buffer,
        .offset = 0,
        .range = sizeof(CameraData)};
    VkWriteDescriptorSet writes[2] = {0};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = app->renderer.gpu->hud_descriptor_set[f];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &uboInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = app->renderer.gpu->hud_descriptor_set[f];
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = MAX_TEXTURE_SLOTS;
    writes[1].pImageInfo = imageInfos;

    vkUpdateDescriptorSets(VK_CTX(app)->logical_device, 2, writes, 0, NULL);
  }
}

void destroy_descriptors(APP *app) {
  vkDestroyDescriptorPool(VK_CTX(app)->logical_device, app->renderer.gpu->descriptor_pool, NULL);
  vkDestroyDescriptorSetLayout(VK_CTX(app)->logical_device, app->renderer.gpu->descriptor_set_layout, NULL);
}

void update_descriptor_texture_slot(APP *app, int slot, Texture *texture) {
  // Aspetta che nessun frame in volo stia leggendo il descriptor set
  // prima di sovrascrivere questo slot specifico. Costoso se chiamata
  // molte volte di fila (es. caricando un livello con tante texture
  // diverse) — per quel caso, texture_manager_begin_batch/end_batch fa
  // un solo wait per l'intero gruppo invece di uno a testa.
  vkDeviceWaitIdle(VK_CTX(app)->logical_device);
  update_descriptor_texture_slot_no_wait(app, slot, texture);
}

void update_descriptor_texture_slot_no_wait(APP *app, int slot,
                                            Texture *texture) {
  VkDescriptorImageInfo imageInfo = {
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .imageView = texture->view,
      .sampler = texture->sampler,
  };

  // Un write per ogni set del compositing, del primo pass e dell'HUD.
  // Tutti condividono il catalogo di texture; aggiornare solo uno dei
  // set renderebbe le texture appena caricate intermittenti tra i frame.
  VkWriteDescriptorSet writes[MAX_FRAMES_IN_FLIGHT] = {0};

  for (int f = 0; f < MAX_FRAMES_IN_FLIGHT; f++) {
    writes[f].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[f].dstSet = app->renderer.gpu->hud_descriptor_set[f];
    writes[f].dstBinding = 1;
    writes[f].dstArrayElement = slot;
    writes[f].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[f].descriptorCount = 1;
    writes[f].pImageInfo = &imageInfo;
  }

  vkUpdateDescriptorSets(VK_CTX(app)->logical_device,
                        MAX_FRAMES_IN_FLIGHT,
                        writes, 0, NULL);
}
