#include "render/vulkan_texture.h"
#include "render/gfx_vulkan.h"

#include "app/application.h"
#include "log/log.h"
#include "render/vulkan_buffer.h"
#include "render/vulkan_memory_tracker.h"

#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool transition_image_layout(APP *app, VkImage image,
                                    VkImageLayout oldLayout,
                                    VkImageLayout newLayout,
                                    uint32_t levelCount) {
  VkCommandBuffer cmd = begin_single_time_commands(app);
  if (!cmd) return false;

  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,

      .oldLayout = oldLayout,
      .newLayout = newLayout,

      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

      .image = image,

      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,

      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = levelCount,

      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1};

  VkPipelineStageFlags srcStage;
  VkPipelineStageFlags dstStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    LOG_ERROR("unsupported image layout transition");
    vkFreeCommandBuffers(VK_CTX(app)->logical_device, VK_CTX(app)->render_context.commands.pool, 1, &cmd);
    return false;
  }

  vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1,
                       &barrier);

  return end_single_time_commands(app, cmd);
}

static bool copy_buffer_to_image(APP *app, VkBuffer buffer, VkImage image,
                                 uint32_t width, uint32_t height) {

  VkCommandBuffer cmd = begin_single_time_commands(app);
  if (!cmd) return false;

  VkBufferImageCopy region = {

      .bufferOffset = 0,

      .bufferRowLength = 0,
      .bufferImageHeight = 0,

      .imageSubresource =
          {

              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,

              .mipLevel = 0,

              .baseArrayLayer = 0,

              .layerCount = 1},

      .imageOffset = {0, 0, 0},

      .imageExtent = {width, height, 1}};

  vkCmdCopyBufferToImage(cmd, buffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  return end_single_time_commands(app, cmd);
}

/* Livelli di mipmap per una texture larga w e alta h: si dimezza fino a
 * 1x1. Zero solo se il formato non sa fare il blit lineare, che e' il modo
 * con cui i livelli vengono generati qui sotto. */
static uint32_t mip_levels_for(APP *app, uint32_t w, uint32_t h) {
  VkFormatProperties props;
  vkGetPhysicalDeviceFormatProperties(VK_CTX(app)->physical_device,
                                      VK_FORMAT_R8G8B8A8_UNORM, &props);
  if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    return 1; /* niente blit lineare: si resta a un livello, come prima */
  uint32_t m = 1;
  uint32_t side = w > h ? w : h;
  while (side > 1) {
    side >>= 1;
    ++m;
  }
  return m;
}

/* Genera i livelli con blit successivi, dimezzando ogni volta. Alla fine
 * TUTTI i livelli restano in SHADER_READ_ONLY_OPTIMAL.
 *
 * Perche' servono: senza mipmap una superficie rimpicciolita campiona pixel
 * sparsi e viene fuori brulicante. Misurato su questo progetto: una cassa
 * del kit guerra ha 256 texel su 87 pixel di schermo, cioe' tre texel per
 * pixel -- non poca risoluzione, troppa, compressa a forza. */
static bool generate_mipmaps(APP *app, Texture *texture) {
  VkCommandBuffer cmd = begin_single_time_commands(app);
  if (!cmd) return false;
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .image = texture->image,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .subresourceRange.levelCount = 1,
      .subresourceRange.layerCount = 1};

  int32_t w = (int32_t)texture->width, h = (int32_t)texture->height;
  for (uint32_t i = 1; i < texture->mipLevels; ++i) {
    /* il livello precedente diventa sorgente del blit */
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    const int32_t nw = w > 1 ? w / 2 : 1;
    const int32_t nh = h > 1 ? h / 2 : 1;
    VkImageBlit blit = {
        .srcOffsets = {{0, 0, 0}, {w, h, 1}},
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1},
        .dstOffsets = {{0, 0, 0}, {nw, nh, 1}},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1}};
    vkCmdBlitImage(cmd, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    /* fatto: quel livello passa a sola lettura dagli shader */
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    w = nw;
    h = nh;
  }
  /* l'ultimo livello non e' mai stato sorgente: va portato a mano */
  barrier.subresourceRange.baseMipLevel = texture->mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
  return end_single_time_commands(app, cmd);
}

static bool create_image(APP *app, Texture *texture, const char *filename) {

  VkImageCreateInfo info = {

      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,

      .imageType = VK_IMAGE_TYPE_2D,

      .extent = {texture->width, texture->height, 1},

      .mipLevels = texture->mipLevels,
      .arrayLayers = 1,

      .format = VK_FORMAT_R8G8B8A8_UNORM,

      .tiling = VK_IMAGE_TILING_OPTIMAL,

      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,

      /* TRANSFER_SRC serve perche' i livelli si generano copiando da un
         livello all'altro con vkCmdBlitImage: l'immagine e' sorgente di se
         stessa. */
      .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,

      .samples = VK_SAMPLE_COUNT_1_BIT,

      .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

  if (vkCreateImage(VK_CTX(app)->logical_device, &info, NULL,
                    &texture->image) != VK_SUCCESS) {
    LOG_ERROR("create image failed");
    texture->image = VK_NULL_HANDLE;
    return false;
  }

  VkMemoryRequirements mem;

  vkGetImageMemoryRequirements(VK_CTX(app)->logical_device, texture->image,
                               &mem);

  uint32_t memory_type_index =
      find_memory_type(VK_CTX(app)->physical_device, mem.memoryTypeBits,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VkMemoryAllocateInfo alloc = {

      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,

      .allocationSize = mem.size,

      .memoryTypeIndex = memory_type_index};

  if (memory_type_index == UINT32_MAX || vkAllocateMemory(VK_CTX(app)->logical_device, &alloc, NULL,
                       &texture->memory) != VK_SUCCESS) {
    LOG_ERROR("texture memory allocation failed");
    texture->memory = VK_NULL_HANDLE;
    return false;
  }

  char label[96];
  snprintf(label, sizeof(label), "texture:%s",
           filename ? filename : "unnamed");
  /* Con le mipmap la memoria vera e' circa 4/3 del livello 0: il
     tracciatore deve dire il vero, o il bilancio della GPU e' una bugia. */
  VkDeviceSize raw_size = (VkDeviceSize)texture->width * texture->height * 4u;
  if (texture->mipLevels > 1)
    raw_size = raw_size * 4u / 3u;
  vulkan_memory_track_allocation(
      VK_CTX(app)->physical_device, texture->memory,
      VULKAN_MEMORY_RESOURCE_IMAGE, label, raw_size, &mem,
      memory_type_index, -1, true);
  vulkan_memory_track_image_info(texture->memory, texture->width,
                                 texture->height, info.format,
                                 info.mipLevels, info.arrayLayers);

  return vkBindImageMemory(VK_CTX(app)->logical_device, texture->image,
                    texture->memory, 0) == VK_SUCCESS;
}

static bool create_view(APP *app, Texture *texture) {

  VkImageViewCreateInfo info = {

      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,

      .image = texture->image,

      .viewType = VK_IMAGE_VIEW_TYPE_2D,

      .format = VK_FORMAT_R8G8B8A8_UNORM,

      .subresourceRange = {

          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,

          .levelCount = texture->mipLevels,

          .layerCount = 1}};

  if (vkCreateImageView(VK_CTX(app)->logical_device, &info, NULL, &texture->view) != VK_SUCCESS) {
    texture->view = VK_NULL_HANDLE; return false;
  }
  return true;
}

static bool create_sampler(APP *app, Texture *texture,
                           VkSamplerAddressMode addressMode) {

  /* Anisotropia: solo se la GPU la espone (ed e' stata chiesta alla
   * creazione del device, vedi vulkan_logical_device.c). Si tiene a 8, che
   * e' il punto oltre il quale non si vede piu' differenza ma si continua a
   * pagare banda. Senza mipmap non servirebbe a niente, per cui e' legata
   * agli stessi casi. */
  float aniso = 1.0f;
  if (texture->mipLevels > 1) {
    VkPhysicalDeviceFeatures feat;
    vkGetPhysicalDeviceFeatures(VK_CTX(app)->physical_device, &feat);
    if (feat.samplerAnisotropy) {
      VkPhysicalDeviceProperties props;
      vkGetPhysicalDeviceProperties(VK_CTX(app)->physical_device, &props);
      aniso = props.limits.maxSamplerAnisotropy;
      if (aniso > 16.0f) /* 2026-10-24: 16, oggetti piu' definiti di taglio */
        aniso = 16.0f;
    }
  }

  VkSamplerCreateInfo info = {

      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,

      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,

      /* LINEAR: si interpola anche FRA due livelli, non solo dentro uno --
         altrimenti al confine fra un livello e l'altro si vede una riga. */
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,

      .addressModeU = addressMode,
      .addressModeV = addressMode,
      .addressModeW = addressMode,

      .anisotropyEnable = aniso > 1.0f ? VK_TRUE : VK_FALSE,
      .maxAnisotropy = aniso,

      .compareEnable = VK_FALSE,

      /* un poco piu' di dettaglio (2026-10-24, "piu' definiti"): il livello
       * di mipmap scelto mezzo gradino piu' fine */
      .mipLodBias = texture->mipLevels > 1 ? -0.5f : 0.0f,
      .minLod = 0.0f,
      .maxLod = (float)texture->mipLevels};
  if (vkCreateSampler(VK_CTX(app)->logical_device, &info, NULL, &texture->sampler) != VK_SUCCESS) {
    texture->sampler = VK_NULL_HANDLE; return false;
  }
  texture->address_mode = addressMode;
  return true;
}

static void load_texture_internal(APP *app, const char *filename,
                                  Texture *texture,
                                  VkSamplerAddressMode addressMode) {

  int channels, width, height;
  VkDevice device = VK_CTX(app)->logical_device;
  VkBuffer staging = VK_NULL_HANDLE;
  VkDeviceMemory staging_memory = VK_NULL_HANDLE;
  unsigned char *pixels = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
  bool ok = false;
  if (!pixels) { LOG_ERROR("cannot load texture %s", filename); return; }
  texture->width = (uint32_t)width; texture->height = (uint32_t)height;
  VkDeviceSize size = (VkDeviceSize)texture->width * texture->height * 4u;
  if (!create_buffer(VK_CTX(app)->physical_device, device, size,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     &staging, &staging_memory)) goto cleanup;
  void *data = NULL;
  if (vkMapMemory(device, staging_memory, 0, size, 0, &data) != VK_SUCCESS) goto cleanup;
  memcpy(data, pixels, (size_t)size);
  vkUnmapMemory(device, staging_memory);
  texture->mipLevels = addressMode == VK_SAMPLER_ADDRESS_MODE_REPEAT
                          ? mip_levels_for(app, texture->width, texture->height) : 1;
  if (!create_image(app, texture, filename) ||
      !transition_image_layout(app, texture->image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mipLevels) ||
      !copy_buffer_to_image(app, staging, texture->image, texture->width, texture->height)) goto cleanup;
  if (texture->mipLevels > 1) {
    if (!generate_mipmaps(app, texture)) goto cleanup;
  } else if (!transition_image_layout(app, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1)) goto cleanup;
  ok = create_view(app, texture) && create_sampler(app, texture, addressMode);
cleanup:
  stbi_image_free(pixels);
  if (staging) vkDestroyBuffer(device, staging, NULL);
  if (staging_memory) {
    vulkan_memory_track_free(staging_memory);
    vkFreeMemory(device, staging_memory, NULL);
  }
  if (!ok) { destroy_texture(app, texture); LOG_ERROR("Texture GPU upload failed: %s", filename); }

}

void load_texture(APP *app, const char *filename, Texture *texture) {
  // CLAMP_TO_EDGE, come sempre — nessun cambiamento di comportamento
  // per nessuna chiamata esistente (in particolare gli atlas animati di
  // player/nemici: con REPEAT, il filtro bilineare ai bordi di un frame
  // "sbircerebbe" nel frame ripetuto adiacente, una cucitura visibile).
  load_texture_internal(app, filename, texture,
                        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
}

// Come load_texture, ma con sampler in modalità REPEAT invece di
// CLAMP_TO_EDGE — per i box con tiling attivo (Box2D.textureTile), la
// cui UV va deliberatamente oltre [0,1] (vedi batch_add_box in
// box_renderer.c) proprio per farla ripetere invece di stirarsi. NON
// va usata per texture con una sotto-regione di atlas (uv_min/uv_max
// che selezionano un frame dentro una texture più grande) — vedi sopra
// il perché.
void load_texture_tiled(APP *app, const char *filename, Texture *texture) {
  load_texture_internal(app, filename, texture,
                        VK_SAMPLER_ADDRESS_MODE_REPEAT);
}

void destroy_texture(APP *app, Texture *texture) {
  VkDevice device = VK_CTX(app)->logical_device;

  if (texture->sampler)
    vkDestroySampler(device, texture->sampler, NULL);

  if (texture->view)
    vkDestroyImageView(device, texture->view, NULL);

  if (texture->image)
    vkDestroyImage(device, texture->image, NULL);

  if (texture->memory)
    vulkan_memory_track_free(texture->memory);
  if (texture->memory)
    vkFreeMemory(device, texture->memory, NULL);
  *texture = (Texture){0};
}

// Ricrea SOLO il sampler di una texture già caricata, con una nuova
// modalità di indirizzamento — immagine e vista restano quelle che
// erano, molto più economico di un reload completo da disco. Serve
// quando una texture era già stata caricata con un sampler CLAMP (da
// un box senza tiling, o da texture_manager_load "normale") e ora
// serve REPEAT per un box con tiling attivo sullo STESSO path — senza
// questo, il riuso dello slot per path (texture_manager_load_internal)
// lascerebbe il vecchio sampler, e il tiling non avrebbe alcun effetto
// visibile (la UV va correttamente oltre [0,1], ma CLAMP blocca tutto
// quello che sfora sul pixel di bordo).
void set_texture_address_mode(APP *app, Texture *texture,
                              VkSamplerAddressMode addressMode) {
  if (texture->address_mode == addressMode) return;
  Texture replacement = *texture;
  replacement.sampler = VK_NULL_HANDLE;
  if (!create_sampler(app, &replacement, addressMode)) return;
  vkDestroySampler(VK_CTX(app)->logical_device, texture->sampler, NULL);
  texture->sampler = replacement.sampler;
  texture->address_mode = addressMode;
}
