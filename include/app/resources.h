#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct APP APP;

/* NULL fields select the repository-compatible defaults. Relative filenames
 * are resolved under root_directory; a relative root is relative to the
 * executable directory, never the working directory. Absolute filenames
 * override the root. Strings are borrowed and must remain valid and unchanged
 * until app_shutdown (also used when recreating the Vulkan pipeline).
 * atlas dimensions default independently to 1024 when zero. */
typedef struct {
    const char *root_directory;
    const char *font_metadata;
    const char *font_atlas;
    const char *vertex_shader;
    const char *fragment_shader;
    uint32_t atlas_width, atlas_height;
} AppResources;

typedef enum {
    APP_RESOURCE_FONT_METADATA,
    APP_RESOURCE_FONT_ATLAS,
    APP_RESOURCE_VERTEX_SHADER,
    APP_RESOURCE_FRAGMENT_SHADER
} AppResource;

/* Resolves a filename without opening it. On failure clears a nonempty output
 * buffer. Empty configured strings, unknown resources and truncation fail.
 * Like the application lifecycle, call from the main thread. */
bool app_resource_path(const APP *app, AppResource resource,
                       char *out, size_t out_size);
