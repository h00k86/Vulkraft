
#pragma once
/* PORTING (E14, 2026-09-25): niente <vulkan/vulkan.h>. Nessuna firma qui
 * sotto nomina Vulkan -- l'include c'era per abitudine, e siccome app.c,
 * performance_lab.c, frame_spike_capture.c ed editor_hud_toolbar.c
 * includono questo header, si portava dietro l'intera API grafica. */
#include <stdint.h>
#include <stdbool.h>
#include "log/log.h"


typedef struct APP APP;

typedef struct {
  uint32_t draw_calls;
  uint32_t hud_draw_calls;
  uint32_t pipeline_binds;
  uint32_t upload_copies;
  uint32_t transfer_submissions;
  uint32_t graphics_submissions;
  uint64_t transfer_bytes;
} RenderCommandStats;

void create_command_buffers(APP *app);

bool submit_pending_uploads(APP *app, uint32_t frame_index);

void record_command_buffer(APP *app,
                            uint32_t frame_index,
                            uint32_t image_index);

void destroy_command_buffers(APP *app);
RenderCommandStats render_command_get_stats(void);
