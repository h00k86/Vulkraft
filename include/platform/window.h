/* Interfaccia della piattaforma: nessun tipo della libreria nativa attraversa
 * questo header. Tutte le operazioni e le callback appartengono al thread principale. */
#pragma once
#include <stdbool.h>
#include "platform/events.h"
typedef struct PlatformWindow PlatformWindow;
typedef struct APP APP;
typedef enum { PLATFORM_WINDOWED, PLATFORM_FULLSCREEN, PLATFORM_BORDERLESS } PlatformWindowMode;
typedef struct {
  const char *title;
  int width, height;
  PlatformWindowMode mode;
  bool hidden;
} PlatformWindowConfig;
/* Main-thread API. Select one implementation at link time. */
/* Crea una finestra posseduta dal chiamante; NULL segnala un fallimento.
 * config e letto durante la chiamata. Distruggere solo handle validi, una volta;
 * destroy_window(NULL) e consentito. I getter richiedono una finestra viva. */
PlatformWindow *platform_window_create(const PlatformWindowConfig *config);
void destroy_window(PlatformWindow *window);
/* Application adapter: translates settings and installs framebuffer resize handling. */
/* Questa variante riserva il puntatore utente ad APP e installa la callback
 * resize del framework: non sovrascrivere quel puntatore dal codice applicativo. */
PlatformWindow *create_window(APP *app);
void platform_poll_events(void);
/* Il backend grafico attende eventi; quello headless svuota solo il batch
 * di test e non blocca. Il tempo headless avanza esplicitamente nei test. */
void platform_wait_events(void);
double platform_time(void);
int platform_window_should_close(PlatformWindow *window);
void platform_window_set_should_close(PlatformWindow *window, int close);
/* Dimensioni logiche per layout e coordinate del mouse. Su HiDPI possono
 * differire dai pixel restituiti da platform_framebuffer_size. */
void platform_window_size(PlatformWindow *window, int *width, int *height);
void platform_framebuffer_size(PlatformWindow *window, int *width, int *height);
void platform_cursor_position(PlatformWindow *window, double *x, double *y);
int platform_mouse_button(PlatformWindow *window, int button);
/* Polling dello stato attuale, non della storia degli eventi: un tap completo
 * tra due frame puo risultare gia rilasciato. Per l'editing usare gli eventi. */
int platform_key_state(PlatformWindow *window, int key);
/* Main thread, live window. Read is borrowed until the next clipboard operation
 * or window destruction. NULL/false means unavailable or failed. */
const char *platform_clipboard_text(PlatformWindow *window);
bool platform_clipboard_set_text(PlatformWindow *window, const char *text);
void platform_window_set_user_pointer(PlatformWindow *window, void *user);
void *platform_window_user_pointer(PlatformWindow *window);
typedef void (*PlatformKeyCallback)(PlatformWindow *,int key,int scancode,int action,int mods);
typedef void (*PlatformCharCallback)(PlatformWindow *,unsigned int codepoint);
typedef void (*PlatformFocusCallback)(PlatformWindow *,int focused);
typedef void (*PlatformScrollCallback)(PlatformWindow *,double x,double y);
typedef void (*PlatformResizeCallback)(PlatformWindow *,int width,int height);
typedef void (*PlatformIconifyCallback)(PlatformWindow *,int iconified);
/* Setters return the old callback; observers must chain it. Native callbacks
 * belong to the backend and must never be replaced by application code. */
PlatformKeyCallback platform_set_key_callback(PlatformWindow *,PlatformKeyCallback);
PlatformCharCallback platform_set_char_callback(PlatformWindow *,PlatformCharCallback);
PlatformFocusCallback platform_set_focus_callback(PlatformWindow *,PlatformFocusCallback);
PlatformScrollCallback platform_set_scroll_callback(PlatformWindow *,PlatformScrollCallback);
PlatformResizeCallback platform_set_resize_callback(PlatformWindow *,PlatformResizeCallback);
PlatformIconifyCallback platform_set_iconify_callback(PlatformWindow *,PlatformIconifyCallback);

/* Optional observer for the ordered event stream. It does not consume events.
 * Defer window destruction until polling returns. All callbacks run on the main thread. */
typedef void (*PlatformEventCallback)(PlatformWindow *, const PlatformEvent *);
/* L'evento passato all'osservatore e valido soltanto durante la callback:
 * copiarlo se serve conservarlo. La callback generale osserva, non consuma. */
PlatformEventCallback platform_set_event_callback(PlatformWindow *,PlatformEventCallback);
