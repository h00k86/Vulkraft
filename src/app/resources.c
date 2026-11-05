#include "app/application.h"
#include "platform/paths.h"
#include "platform/platform_fs.h"
#include <stdio.h>
#include <string.h>

static bool absolute_path(const char *path) {
    if (path[0] == '/') return true;
#ifdef _WIN32
    if (path[0] == '\\') return true;
    if (((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\')) return true;
#endif
    return false;
}

bool app_resource_path(const APP *app, AppResource resource,
                       char *out, size_t out_size) {
    if (!out || !out_size) return false;
    out[0] = '\0';
    if (!app) return false;
    const AppResources *r = &app->resources;
    const char *path;
    switch (resource) {
    case APP_RESOURCE_FONT_METADATA:
        path = r->font_metadata ? r->font_metadata : "assets/fonts/font_utf8.json"; break;
    case APP_RESOURCE_FONT_ATLAS:
        path = r->font_atlas ? r->font_atlas : "assets/fonts/font_utf8.png"; break;
    case APP_RESOURCE_VERTEX_SHADER:
        path = r->vertex_shader ? r->vertex_shader : "shaders/vert.spv"; break;
    case APP_RESOURCE_FRAGMENT_SHADER:
        path = r->fragment_shader ? r->fragment_shader : "shaders/frag.spv"; break;
    default: return false;
    }
    if (!*path) return false;
    int n;
    if (absolute_path(path)) {
        n = snprintf(out, out_size, "%s", path);
    } else if (!r->root_directory) {
        if (paths_engine_file(out, out_size, path)) return true;
        out[0] = '\0';
        return false;
    } else {
        if (!*r->root_directory) return false;
        if (absolute_path(r->root_directory)) {
            n = snprintf(out, out_size, "%s/%s", r->root_directory, path);
        } else {
            char exe[APP_PATH_MAX];
            if (!platform_exe_dir(exe, sizeof(exe))) return false;
            n = snprintf(out, out_size, "%s/%s/%s", exe, r->root_directory, path);
        }
    }
    if (n >= 0 && (size_t)n < out_size) return true;
    out[0] = '\0';
    return false;
}
