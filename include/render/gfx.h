#pragma once
#include "math/matrix.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MAX_FRAMES_IN_FLIGHT 4
#define MAX_TEXTURE_SLOTS 16
// Opaque ownership: only src/render knows the Vulkan definitions.
typedef struct APP APP;
typedef struct VulkanContext VulkanContext;
typedef struct RenderContext RenderContext;
typedef struct RendererGpu RendererGpu;
typedef void *GfxCommandList;
typedef struct {
    float pos[3];
    float color[4];
    float uv[2];
    int textureID;
} Vertex;
typedef struct CameraData {
    mat4 projection;
    mat4 view;
} CameraData;
/* Alloca i contenitori grafici e avvia il backend. Su errore esegue rollback
 * delle risorse gia create; il dettaglio resta in APP.error. */
bool gfx_init(APP *app);
/* Distrugge le risorse GPU e la finestra, poi libera i contenitori opachi.
 * Per l'app completa usare app_shutdown, che scollega anche i gestori input. */
void gfx_shutdown(APP *app);
bool gfx_device_ready(const APP *app);
void gfx_wait_idle(APP *app);
uint32_t gfx_current_frame(const APP *app);
void gfx_recreate_surface(APP *app);
void gfx_surface_size(const APP *app, uint32_t *w, uint32_t *h);
const char *gfx_selected_gpu_name(void);
// False means a fatal error recorded in APP.error: stop rendering and shut down.
// True from draw_frame also covers a skipped frame while recreating the surface.
bool wait_for_current_frame(APP *app);
bool draw_frame(APP *app);
