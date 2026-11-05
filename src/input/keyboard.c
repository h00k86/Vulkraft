#include "input/keyboard.h"
#include "log/log.h"
#include "platform/window.h"
#include <string.h>

/* Gestore globale per una finestra: pending raccoglie le callback durante il
 * polling, frame espone un batch stabile ai widget fino al frame successivo. */
static PlatformWindow *keyboard_window;
static PlatformKeyCallback previous_key;
static PlatformCharCallback previous_character;
static PlatformFocusCallback previous_focus;
static InputKeyEvent pending_events[INPUT_KEYBOARD_CAPACITY], frame_events[INPUT_KEYBOARD_CAPACITY];
static size_t pending_count, frame_count;
static bool pending_lost_focus, frame_lost_focus, overflow, unfocused;

static void enqueue(InputKeyEvent event) {
  if (unfocused || pending_lost_focus) return;
  if (pending_count == INPUT_KEYBOARD_CAPACITY) { overflow=true; return; }
  pending_events[pending_count++]=event;
}
static void input_keyboard_on_key(PlatformWindow *w,int key,int scan,int action,int mods) {
  if (w==keyboard_window)
    enqueue((InputKeyEvent){.kind=INPUT_KEY,.key=key,.action=action,.mods=mods});
  if (previous_key) previous_key(w,key,scan,action,mods);
}
/* Un evento carattere contiene un punto di codice Unicode, non un tasto
 * fisico: layout, accenti e caratteri confermati sono responsabilita della piattaforma. */
static void input_keyboard_on_character(PlatformWindow *w,unsigned int codepoint) {
  if (w==keyboard_window)
    enqueue((InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint=codepoint});
  if (previous_character) previous_character(w,codepoint);
}
static void input_keyboard_on_focus(PlatformWindow *w,int focused) {
  if (w==keyboard_window) {
    unfocused=!focused;
    if (unfocused) {
      // Non applicare comandi rimasti in coda a un editor che ha perso il focus.
      pending_count=0; pending_lost_focus=true;
    }
  }
  if (previous_focus) previous_focus(w,focused);
}
void input_keyboard_register(PlatformWindow *window) {
  if (!window || keyboard_window==window) return;
  if (keyboard_window) input_keyboard_unregister(keyboard_window);
  keyboard_window=window;
  pending_count=frame_count=0;
  pending_lost_focus=frame_lost_focus=overflow=unfocused=false;
  previous_key=platform_set_key_callback(window,input_keyboard_on_key);
  previous_character=platform_set_char_callback(window,input_keyboard_on_character);
  previous_focus=platform_set_focus_callback(window,input_keyboard_on_focus);
}
void input_keyboard_unregister(PlatformWindow *window) {
  if (!window || window!=keyboard_window) return;
  // Registration owns these callback slots until unregistration. Restore the
  // original callbacks, also removing any later diagnostic wrappers.
  platform_set_key_callback(window,previous_key);
  platform_set_char_callback(window,previous_character);
  platform_set_focus_callback(window,previous_focus);
  keyboard_window=NULL;
  previous_key=NULL; previous_character=NULL; previous_focus=NULL;
  pending_count=frame_count=0;
  pending_lost_focus=frame_lost_focus=overflow=unfocused=false;
}
void input_keyboard_begin_frame(PlatformWindow *window) {
  frame_count=0; frame_lost_focus=false;
  if (!window || window!=keyboard_window) return;
  /* Never apply a truncated batch of editing commands. Diagnose overload and
   * release editor focus instead. Typical bursts are far below this bound. */
  frame_lost_focus=pending_lost_focus || overflow || unfocused;
  if (overflow) LOG_ERROR("Keyboard event queue exceeded %d events; batch discarded",INPUT_KEYBOARD_CAPACITY);
  if (!frame_lost_focus) {
    memcpy(frame_events,pending_events,pending_count*sizeof(*frame_events)); frame_count=pending_count;
  }
  pending_count=0; pending_lost_focus=overflow=false;
}
/* Memoria posseduta dal modulo: il consumatore puo marcare consumed, ma non
 * deve liberare o conservare il puntatore oltre il successivo begin_frame. */
InputKeyEvent *input_keyboard_events(size_t *count) { *count=frame_count; return frame_events; }
bool input_keyboard_focus_lost(void) { return frame_lost_focus; }
