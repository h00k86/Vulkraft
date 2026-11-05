#include "app/application.h"
#include "platform/window.h"
#include "render/text_renderer.h"
#include "ui/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned frames; bool smoke; } ExampleState;

/* Application state belongs to main; the framework borrows it during app_run. */
static void draw(APP *app, void *user_data) {
    ExampleState *state = user_data;
    Font *font = &app->renderer.font;
    draw_quad(app, 0, 0, VIRTUAL_W, VIRTUAL_H, 0.08f, 0.09f, 0.12f);
    draw_text_colored(app, font, "Applicazione esterna - UTF-8: caffè", 30, 55, 24, 1, 1, 1);
    UIButtonStyle style = {.min_width = 160, .min_height = 36};
    if (ui_button_labeled("close", "Chiudi", font, ANCHOR_TOP_LEFT, 0, 18, 30, 85, &style))
        app_request_close(app);
    if (++state->frames == 16 && state->smoke)
        app_request_close(app);
}

int main(int argc, char **argv) {
    APP app = {0};
    ExampleState state = {0};
    const AppCallbacks callbacks = {.draw = draw};
    app.title = "External GraphicsApp";
    app.resources.root_directory = "resources";
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--smoke")) state.smoke = true;
        else if (!strcmp(argv[i], "--resources") && i + 1 < argc)
            app.resources.root_directory = argv[++i];
        else if (!strcmp(argv[i], "--font-metadata") && i + 1 < argc)
            app.resources.font_metadata = argv[++i];
        else if (!strcmp(argv[i], "--font-atlas") && i + 1 < argc)
            app.resources.font_atlas = argv[++i];
        else if (!strcmp(argv[i], "--vertex-shader") && i + 1 < argc)
            app.resources.vertex_shader = argv[++i];
        else if (!strcmp(argv[i], "--fragment-shader") && i + 1 < argc)
            app.resources.fragment_shader = argv[++i];
        else {
            fprintf(stderr, "Usage: %s [--smoke] [--resources directory]\n"
                            "  [--font-metadata file] [--font-atlas file]\n"
                            "  [--vertex-shader file] [--fragment-shader file]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    bool ready = app_init(&app);
    if (ready) app_run(&app, &callbacks, &state);
    app_shutdown(&app);
    if (!ready || app.error.code) {
        fprintf(stderr, "Application error %d: %s\n", app.error.code,
                app.error.stage ? app.error.stage : "unknown");
        return EXIT_FAILURE;
    }
    printf("External application completed: %u frames\n", state.frames);
    return EXIT_SUCCESS;
}
