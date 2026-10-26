#pragma once
#include "render/gfx.h"
#include "render/font.h"
#include <stdbool.h>
typedef struct GLFWwindow GLFWwindow;
extern float g_virtualWidth;
extern float g_virtualHeight;
#define VIRTUAL_W g_virtualWidth
#define VIRTUAL_H g_virtualHeight
#define WORLD_Z_MIN -1000.0f
#define WORLD_Z_MAX 1000.0f
// Retained video fields from GameSettings; no gameplay state.
typedef enum { WINDOW_MODE_WINDOWED, WINDOW_MODE_FULLSCREEN, WINDOW_MODE_BORDERLESS } WindowMode;
typedef struct { int width, height; WindowMode window_mode; } VideoSettings;
typedef struct { VideoSettings video; } AppSettings;
typedef struct {
    RendererGpu *gpu;
    CameraData camera_data;
    Font font;
} Renderer;
typedef struct APP {
    GLFWwindow *window;
    const char *title;
    bool framebuffer_resized;
    VulkanContext *context;
    Renderer renderer;
    AppSettings settings;
    struct { float dt, totalTime; } time;
} APP;
void app_init(APP *app);
void app_shutdown(APP *app);
void app_loop(APP *app);
// Composition seam: the example supplies the engine's former game_draw_hud.
void app_draw(APP *app);
