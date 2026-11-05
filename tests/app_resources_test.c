/* Path contracts are independent of a window, working directory and GPU. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "app/application.h"
#include "platform/paths.h"
#include "platform/platform_fs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    APP a = {0}, b = {0};
    char path[APP_PATH_MAX], expected[APP_PATH_MAX], exe[APP_PATH_MAX];
    const char *defaults[] = {"assets/fonts/font_utf8.json", "assets/fonts/font_utf8.png",
                              "shaders/vert.spv", "shaders/frag.spv"};
    for (int i = 0; i < 4; ++i) {
        assert(paths_engine_file(expected, sizeof(expected), defaults[i]));
        assert(app_resource_path(&a, (AppResource)i, path, sizeof(path)));
        assert(!strcmp(path, expected));
    }
    assert(platform_exe_dir(exe, sizeof(exe)));
    a.resources.root_directory = "app assets";
    a.resources.font_metadata = "fonts/custom.json";
    int n = snprintf(expected, sizeof(expected), "%s/app assets/fonts/custom.json", exe);
    assert(n > 0 && (size_t)n < sizeof(expected));
    assert(app_resource_path(&a, APP_RESOURCE_FONT_METADATA, path, sizeof(path)));
    assert(!strcmp(path, expected));
    b.resources.root_directory = "/other/application";
    assert(app_resource_path(&b, APP_RESOURCE_FONT_ATLAS, path, sizeof(path)));
    assert(!strcmp(path, "/other/application/assets/fonts/font_utf8.png"));
    assert(app_resource_path(&a, APP_RESOURCE_FONT_METADATA, path, sizeof(path)));
    assert(!strcmp(path, expected)); /* No global root is changed by b. */
    a.resources.vertex_shader = "/custom shaders/ui.vert.spv";
    assert(app_resource_path(&a, APP_RESOURCE_VERTEX_SHADER, path, sizeof(path)));
    assert(!strcmp(path, "/custom shaders/ui.vert.spv"));
    a.resources.root_directory = "";
    assert(app_resource_path(&a, APP_RESOURCE_VERTEX_SHADER, path, sizeof(path)));
    assert(!app_resource_path(&a, APP_RESOURCE_FONT_METADATA, path, sizeof(path)) && !path[0]);
    a.resources.fragment_shader = "";
    assert(!app_resource_path(&a, APP_RESOURCE_FRAGMENT_SHADER, path, sizeof(path)) && !path[0]);
    assert(!app_resource_path(&b, APP_RESOURCE_FONT_ATLAS, path, 2) && !path[0]);
    assert(!app_resource_path(&a, (AppResource)99, path, sizeof(path)) && !path[0]);
    assert(!app_resource_path(NULL, APP_RESOURCE_FONT_ATLAS, path, sizeof(path)) && !path[0]);
    assert(!app_resource_path(&a, APP_RESOURCE_FONT_ATLAS, NULL, 1));
    assert(!app_resource_path(&a, APP_RESOURCE_FONT_ATLAS, path, 0));
    puts("Application resource path contracts: PASS");
    return 0;
}
