#include "app/application.h"
#include "platform/paths.h"
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  (void)argc; (void)argv;
  paths_engine_dir_init();
  APP *app = malloc(sizeof(APP));
  if (!app) return EXIT_FAILURE;
  memset(app, 0, sizeof(APP));
  app_init(app);
  app_loop(app);
  app_shutdown(app);
  free(app);
  return EXIT_SUCCESS;
}
