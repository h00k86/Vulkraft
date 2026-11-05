#include <stddef.h>
#include "input/scroll.h"
/* Si accumulano tutti i delta tra due frame; take consegna il totale una sola
 * volta. Register/unregister azzerano il residuo per non riusarlo in una nuova app. */
static float accumulated;
static PlatformWindow *registered_window;
static PlatformScrollCallback previous;
static void on_scroll(PlatformWindow *w,double x,double y) {
  if(w==registered_window) accumulated+=(float)y;
  if(previous) previous(w,x,y);
}
void input_scroll_register(PlatformWindow *window) {
  if(!window || window==registered_window) return;
  if(registered_window) input_scroll_unregister(registered_window);
  registered_window=window; accumulated=0;
  previous=platform_set_scroll_callback(window,on_scroll);
}
/* Ripristinare la callback precedente prima che il backend distrugga la finestra. */
void input_scroll_unregister(PlatformWindow *window) {
  if(!window || window!=registered_window) return;
  platform_set_scroll_callback(window,previous);
  registered_window=NULL; previous=NULL; accumulated=0;
}
float input_scroll_take(void) { float value=accumulated; accumulated=0; return value; }
