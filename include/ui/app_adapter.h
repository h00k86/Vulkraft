#pragma once
#include <stdbool.h>
#include "ui/renderer.h"
typedef struct APP APP;
/* Collega il contesto APP al renderer di produzione. Risorse prese in prestito. */
bool ui_bind_app_renderer(APP *app);
/* Bind host clipboard to app->ui; window must outlive the binding. */
bool ui_bind_app_clipboard(APP *app);

/* Tabella immutabile: user deve essere una APP viva. */
const UIRenderer *ui_app_renderer(void);

/* With a custom app loop, register input_keyboard and input_scroll callbacks
 * before polling. Call once per rendered frame after event polling; this consumes
 * keyboard and scroll together. Do not take/apply scroll separately afterwards.
 * Text editing accepts Unicode
 * scalars from the platform and stores UTF-8. Cursor/deletion units are scalars, not
 * grapheme clusters. IME preedit/shaping are not provided. */
void ui_begin_frame(APP *app, float mouse_x, float mouse_y, bool mouse_down,
                    float delta_time);
