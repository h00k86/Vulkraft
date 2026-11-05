/* Keep the legacy global hook in its own archive object: callback consumers
 * must link successfully without defining app_draw, even without section GC. */
#include "app/application.h"
#include <stddef.h>

static void legacy_draw(APP *app, void *user_data) {
    (void)user_data;
    app_draw(app);
}

void app_loop(APP *app) {
    const AppCallbacks callbacks = {.draw = legacy_draw};
    app_run(app, &callbacks, NULL);
}
