#include "app/application.h"
#include "platform/paths.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Esempio minimo del ciclo di vita: APP azzerata -> init -> loop -> shutdown.
 * L'eseguibile decide lo stato di uscita; le funzioni del framework non
 * terminano il processo quando il rendering incontra un errore. */
int main(int argc, char **argv) {
  (void)argc; (void)argv;
  paths_engine_dir_init();
  APP *app = malloc(sizeof(APP));
  if (!app) return EXIT_FAILURE;
  memset(app, 0, sizeof(APP));
  if (!app_init(app)) {
    fprintf(stderr, "Initialization failed: %s (code %d)\n", app->error.stage, app->error.code);
    app_shutdown(app);
    free(app);
    return EXIT_FAILURE;
  }
  app_loop(app);
  int result = app->error.code ? EXIT_FAILURE : EXIT_SUCCESS;
  if (result != EXIT_SUCCESS)
    fprintf(stderr, "Rendering failed: %s (code %d)\n", app->error.stage, app->error.code);
  app_shutdown(app);
  free(app);
  return result;
}
