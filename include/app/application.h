/* Contratto applicativo: coordina UI, input e grafica senza esporre handle nativi.
 * Il framework supporta attualmente una sola applicazione/UI attiva alla volta. */
#pragma once
#include "render/gfx.h"
#include "render/font.h"
#include "app/resources.h"
#include <stdbool.h>
typedef struct PlatformWindow PlatformWindow;
typedef struct UIContext UIContext;
extern float g_virtualWidth;
extern float g_virtualHeight;
#define VIRTUAL_W g_virtualWidth
#define VIRTUAL_H g_virtualHeight
#define WORLD_Z_MIN -1000.0f
#define WORLD_Z_MAX 1000.0f
// Application video configuration.
typedef enum { WINDOW_MODE_WINDOWED, WINDOW_MODE_FULLSCREEN, WINDOW_MODE_BORDERLESS } WindowMode;
typedef struct { int width, height; WindowMode window_mode; } VideoSettings;
typedef struct { VideoSettings video; } AppSettings;
typedef struct {
    RendererGpu *gpu;
    CameraData camera_data;
    Font font;
} Renderer;
typedef enum { APP_ERROR_NONE, APP_ERROR_STATE, APP_ERROR_MEMORY,
               APP_ERROR_WINDOW, APP_ERROR_GRAPHICS, APP_ERROR_RESOURCE } AppErrorCode;
/* stage indica la fase fallita: e un puntatore a testo statico, non una stringa
 * da liberare. Il chiamante controlla error dopo init e dopo il loop. */
typedef struct { AppErrorCode code; const char *stage; } AppError;
// Initialize from zero; a failed initialization can be retried after fixing its cause.
typedef struct APP {
    AppError error;
    bool initialized;
    bool running; /* Managed by the framework; do not modify. */
    PlatformWindow *window;
    UIContext *ui; /* Posseduto da APP; distrutto prima della grafica. */
    const char *title;
    bool framebuffer_resized;
    VulkanContext *context;
    Renderer renderer;
    AppSettings settings;
    AppResources resources; /* Configure before init; see app/resources.h. */
    struct { float dt, totalTime; } time;
} APP;
/* Inizializzare APP a zero, poi impostare titolo/video. true significa che
 * font, finestra e risorse grafiche sono pronti. Anche dopo false chiamare
 * app_shutdown; non richiamare init su un oggetto ancora inizializzato. */
bool app_init(APP *app);
/* Rilascia input e grafica; non libera la struttura APP posseduta dal chiamante.
 * Tollera inizializzazione parziale e chiamate ripetute; distrugge la UI di APP. */
void app_shutdown(APP *app);
/* Callbacks run on the main thread. draw is required; close_requested is optional
 * (NULL accepts closure). Return false to defer closure, e.g. for a save dialog.
 * user_data is borrowed and must remain valid until app_run returns. */
typedef struct {
    void (*draw)(APP *app, void *user_data);
    bool (*close_requested)(APP *app, void *user_data);
} AppCallbacks;
/* Copies the callback table at entry. No global app_draw symbol is required.
 * Callbacks must not destroy APP or enter another loop. */
void app_run(APP *app, const AppCallbacks *callbacks, void *user_data);
/* Schedules a close request, handled outside the UI frame by close_requested.
 * The same policy applies to native window close and legacy window close flags. */
void app_request_close(APP *app);
/* Esegue frame finche arriva una richiesta di chiusura o un errore fatale.
 * Non distrugge APP: restituisce il controllo al chiamante per lo shutdown. */
void app_loop(APP *app);
/* Legacy adapter: app_loop uses the executable-provided app_draw.
 * New consumers use app_run and need not implement this symbol. */
void app_draw(APP *app);
