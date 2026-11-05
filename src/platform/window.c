/* Adattatore applicativo comune ai backend: traduce le impostazioni APP,
 * mentre creazione nativa e raccolta eventi restano nelle implementazioni selezionate. */
#include "platform/window.h"
#include "app/application.h"
/* La callback marca soltanto la superficie come obsoleta. La ricreazione GPU
 * avviene nel loop, fuori dal dispatch degli eventi. */
static void resized(PlatformWindow *window,int width,int height) {
  APP *app=platform_window_user_pointer(window);
  if (app) app->framebuffer_resized=true;
}
PlatformWindow *create_window(APP *app) {
  PlatformWindowConfig config={.title=app->title,.width=app->settings.video.width,
    .height=app->settings.video.height,.mode=PLATFORM_WINDOWED};
  if (app->settings.video.window_mode==WINDOW_MODE_FULLSCREEN) config.mode=PLATFORM_FULLSCREEN;
  else if (app->settings.video.window_mode==WINDOW_MODE_BORDERLESS) config.mode=PLATFORM_BORDERLESS;
  PlatformWindow *window=platform_window_create(&config);
  if (window) {
    // Il backend usa un proprio wrapper nativo: questo puntatore resta riservato ad APP.
    platform_window_set_user_pointer(window,app);
    platform_set_resize_callback(window,resized);
    if (config.mode==PLATFORM_FULLSCREEN)
      platform_window_size(window,&app->settings.video.width,&app->settings.video.height);
  }
  return window;
}
