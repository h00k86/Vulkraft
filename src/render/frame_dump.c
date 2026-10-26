// MACROTASK MULTI-GIOCO (2026-09-17): vedi include/render/frame_dump.h.
#include "render/frame_dump.h"
#include "render/gfx_vulkan.h"

#include "log/log.h"
#include "render/vulkan_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDeviceSize size;
  uint32_t width, height;
  VkFormat format;
  bool pending; /* copia registrata, PPM ancora da scrivere */
} FrameDumpSlot;

static bool g_initialized, g_enabled;
static const char *g_dir;
static int g_every = 30, g_max = 100000, g_counter, g_written;
static float g_fps = 0.0f, g_nextTime = 0.0f; /* APP_FRAME_DUMP_FPS: cadenza sul tempo simulato */
static FrameDumpSlot g_slots[MAX_FRAMES_IN_FLIGHT];

static void init_once(void) {
  if (g_initialized)
    return;
  g_initialized = true;
  g_dir = getenv("APP_FRAME_DUMP_DIR");
  g_enabled = g_dir && g_dir[0];
  const char *e = getenv("APP_FRAME_DUMP_EVERY");
  if (e && atoi(e) > 0)
    g_every = atoi(e);
  const char *m = getenv("APP_FRAME_DUMP_MAX");
  if (m && atoi(m) > 0)
    g_max = atoi(m);
  const char *fps = getenv("APP_FRAME_DUMP_FPS");
  if (fps && atof(fps) > 0.0)
    g_fps = (float)atof(fps);
}

static void slot_ensure(APP *app, FrameDumpSlot *s, VkDeviceSize size) {
  if (s->buffer != VK_NULL_HANDLE && s->size >= size)
    return;
  VkDevice device = VK_CTX(app)->logical_device;
  if (s->buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(device, s->buffer, NULL);
    vkFreeMemory(device, s->memory, NULL);
    s->buffer = VK_NULL_HANDLE;
    s->memory = VK_NULL_HANDLE;
  }
  create_buffer(VK_CTX(app)->physical_device, device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &s->buffer, &s->memory);
  s->size = size;
}

void frame_dump_record(APP *app, GfxCommandList cmdIn, uint32_t frame_index,
                       uint32_t image_index) {
  /* PORTING (E14): sopra il confine e' una GfxCommandList opaca; il
   * backend la riconverte nel proprio tipo e da qui in poi nulla
   * cambia rispetto a prima. */
  VkCommandBuffer cmd = (VkCommandBuffer)cmdIn;

  init_once();
  if (!g_enabled || g_written + 1 > g_max || frame_index >= MAX_FRAMES_IN_FLIGHT)
    return;
  if (g_fps > 0.0f) {
    /* Cadenza sul tempo SIMULATO: il video risulta in tempo reale anche se
     * il rendering va piu' veloce o piu' lento di 60 fps. */
    if (app->time.totalTime + 1e-4f < g_nextTime)
      return;
    g_nextTime = app->time.totalTime + 1.0f / g_fps;
  } else if ((g_counter++ % g_every) != 0)
    return;
  RenderContext *rc = &VK_CTX(app)->render_context;
  uint32_t w = rc->swapchain.extent.width, h = rc->swapchain.extent.height;
  if (w == 0 || h == 0)
    return;
  FrameDumpSlot *s = &g_slots[frame_index];
  if (s->pending)
    return; /* lo slot precedente non e' ancora stato scritto */
  slot_ensure(app, s, (VkDeviceSize)w * h * 4);
  s->width = w;
  s->height = h;
  s->format = rc->swapchain.format;
  VkImage image = rc->swapchain.images[image_index];
  VkImageMemoryBarrier toSrc = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toSrc);
  VkBufferImageCopy region = {.bufferOffset = 0,
                              .bufferRowLength = 0,
                              .bufferImageHeight = 0,
                              .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                              .imageOffset = {0, 0, 0},
                              .imageExtent = {w, h, 1}};
  vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s->buffer, 1, &region);
  VkImageMemoryBarrier back = toSrc;
  back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  back.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  back.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       0, 0, NULL, 0, NULL, 1, &back);
  s->pending = true;
}

void frame_dump_collect(APP *app, uint32_t frame_index) {
  if (!g_enabled || frame_index >= MAX_FRAMES_IN_FLIGHT)
    return;
  FrameDumpSlot *s = &g_slots[frame_index];
  if (!s->pending)
    return;
  s->pending = false;
  VkDevice device = VK_CTX(app)->logical_device;
  void *data = NULL;
  if (vkMapMemory(device, s->memory, 0, s->size, 0, &data) != VK_SUCCESS || !data)
    return;
  char path[1024];
  snprintf(path, sizeof(path), "%s/frame_%05d.ppm", g_dir, g_written);
  FILE *f = fopen(path, "wb");
  if (f) {
    uint32_t w = s->width, h = s->height;
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    const unsigned char *px = (const unsigned char *)data;
    bool bgr = s->format == VK_FORMAT_B8G8R8A8_UNORM || s->format == VK_FORMAT_B8G8R8A8_SRGB;
    unsigned char *row = malloc((size_t)w * 3);
    if (row) {
      for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
          const unsigned char *p = px + ((size_t)y * w + x) * 4;
          row[x * 3 + 0] = bgr ? p[2] : p[0];
          row[x * 3 + 1] = p[1];
          row[x * 3 + 2] = bgr ? p[0] : p[2];
        }
        fwrite(row, 1, (size_t)w * 3, f);
      }
      free(row);
    }
    fclose(f);
    ++g_written;
  } else {
    LOG_WARN("frame dump: impossibile scrivere %s", path);
  }
  vkUnmapMemory(device, s->memory);
}

void frame_dump_shutdown(APP *app) {
  if (!g_enabled)
    return;
  VkDevice device = VK_CTX(app)->logical_device;
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (g_slots[i].buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, g_slots[i].buffer, NULL);
      vkFreeMemory(device, g_slots[i].memory, NULL);
    }
    memset(&g_slots[i], 0, sizeof(g_slots[i]));
  }
}
