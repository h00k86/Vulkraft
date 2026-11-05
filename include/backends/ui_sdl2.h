#pragma once
#include <SDL.h>
#include "ui/ui.h"
/* SDL video initialization belongs to the host. One backend owns its offscreen
 * software surface, renderer, FreeType font and preview image (slot zero).
 * Detach/destroy UI context and draw list before destroying the backend. */
typedef struct UISDL2 UISDL2;
UISDL2 *ui_sdl2_create(int width,int height,const char *font_path);
void ui_sdl2_destroy(UISDL2 *backend);
const UIRenderer *ui_sdl2_renderer(void);
Font *ui_sdl2_font(UISDL2 *backend);
/* Resize between frames, after submit/discard; previous surface becomes invalid. */
bool ui_sdl2_resize(UISDL2 *backend,int width,int height);
/* begin rejects an active frame; end requires an active, error-free frame with
 * balanced clips. Failed end leaves the frame active for repair or discard.
 * Resize (even to the same size) is rejected while a frame is active. */
bool ui_sdl2_begin(UISDL2 *backend);
bool ui_sdl2_end(UISDL2 *backend);
/* Abandon a frame without presenting. Resets clips, preserves the last error
 * until begin, and permits resize/restart. Safe for NULL and repeated calls.
 * Discard a pending draw list separately before resizing its target backend. */
void ui_sdl2_discard(UISDL2 *backend);
SDL_Surface *ui_sdl2_surface(UISDL2 *backend);
const char *ui_sdl2_error(const UISDL2 *backend);
/* Input translator: host filters events by window and drains the native queue.
 * Left-button transitions retain their logical pixel positions; frame consumes
 * at most one transition. Call begin/event/frame once per rendered UI frame.
 * begin clears keyboard/wheel input, but preserves queued mouse transitions.
 * Mouse overflow cancels gestures using focus_lost; keyboard overflow rejects frame.
 * Zero-initialize before use. Focus loss drops the batch and suppresses input
 * until focus returns; a loss/gain batch remains cancelled until next begin. */
#define UI_SDL2_MOUSE_CAPACITY 128
typedef struct { float x,y; bool down; } UISDL2MouseTransition;
typedef struct {
  InputKeyEvent events[INPUT_KEYBOARD_CAPACITY];
  size_t count;
  float x,y,scroll;
  bool down,focus_lost,overflow,unfocused;
  UISDL2MouseTransition mouse_events[UI_SDL2_MOUSE_CAPACITY];
  size_t mouse_head,mouse_count;
  bool mouse_observed_down,mouse_cancelled;
} UISDL2Input;
void ui_sdl2_input_begin(UISDL2Input *input);
void ui_sdl2_input_event(UISDL2Input *input,const SDL_Event *event);
bool ui_sdl2_input_frame(UISDL2Input *input,int width,int height,float dt,UIFrameInput *out);
/* SDL video must stay initialized until this binding is detached/context destroyed. */
bool ui_sdl2_bind_clipboard(UIContext *context);
