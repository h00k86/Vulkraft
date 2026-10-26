#include "render/vsync_settings.h"
#include "render/gfx_vulkan.h"

// V-Sync — on di default, stesso comportamento di sempre finché non lo
// tocchi dal menu OPZIONI. Vive qui, non su APP, per lo stesso motivo
// di altre piccole impostazioni sparse in questo progetto: una singola
// bool non merita di allargare uno struct già grande.
static bool g_vsyncEnabled = true;

bool vsync_get_enabled(void) { return g_vsyncEnabled; }

void vsync_set_enabled(bool enabled) { g_vsyncEnabled = enabled; }
