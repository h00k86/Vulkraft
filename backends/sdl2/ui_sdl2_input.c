#include "backends/ui_sdl2.h"
#include "text/utf8.h"
#include <math.h>
#include <string.h>
static int key(SDL_Scancode s) {
  if(s>=SDL_SCANCODE_A && s<=SDL_SCANCODE_Z) return PLATFORM_KEY_A+s-SDL_SCANCODE_A;
  if(s>=SDL_SCANCODE_1 && s<=SDL_SCANCODE_9) return PLATFORM_KEY_1+s-SDL_SCANCODE_1;
  if(s>=SDL_SCANCODE_F1 && s<=SDL_SCANCODE_F12) return PLATFORM_KEY_F1+s-SDL_SCANCODE_F1;
  if(s>=SDL_SCANCODE_F13 && s<=SDL_SCANCODE_F24) return PLATFORM_KEY_F13+s-SDL_SCANCODE_F13;
  if(s>=SDL_SCANCODE_KP_1 && s<=SDL_SCANCODE_KP_9) return PLATFORM_KEY_KP_1+s-SDL_SCANCODE_KP_1;
#define MAP(a,b) case SDL_SCANCODE_##a: return PLATFORM_KEY_##b
  switch(s) {
  MAP(CAPSLOCK,CAPS_LOCK); MAP(SCROLLLOCK,SCROLL_LOCK); MAP(NUMLOCKCLEAR,NUM_LOCK);
  MAP(PRINTSCREEN,PRINT_SCREEN); MAP(PAUSE,PAUSE); MAP(APPLICATION,MENU);
  MAP(KP_0,KP_0); MAP(KP_PERIOD,KP_DECIMAL); MAP(KP_DIVIDE,KP_DIVIDE);
  MAP(KP_MULTIPLY,KP_MULTIPLY); MAP(KP_MINUS,KP_SUBTRACT); MAP(KP_PLUS,KP_ADD);
  MAP(KP_ENTER,KP_ENTER); MAP(KP_EQUALS,KP_EQUAL);
  MAP(0,0); MAP(RETURN,ENTER); MAP(ESCAPE,ESCAPE); MAP(BACKSPACE,BACKSPACE);
  MAP(TAB,TAB); MAP(SPACE,SPACE); MAP(DELETE,DELETE); MAP(INSERT,INSERT);
  MAP(HOME,HOME); MAP(END,END); MAP(PAGEUP,PAGE_UP); MAP(PAGEDOWN,PAGE_DOWN);
  MAP(LEFT,LEFT); MAP(RIGHT,RIGHT); MAP(UP,UP); MAP(DOWN,DOWN);
  MAP(LCTRL,LEFT_CONTROL); MAP(RCTRL,RIGHT_CONTROL); MAP(LSHIFT,LEFT_SHIFT); MAP(RSHIFT,RIGHT_SHIFT);
  MAP(LALT,LEFT_ALT); MAP(RALT,RIGHT_ALT); MAP(LGUI,LEFT_SUPER); MAP(RGUI,RIGHT_SUPER);
  MAP(MINUS,MINUS); MAP(EQUALS,EQUAL); MAP(COMMA,COMMA); MAP(PERIOD,PERIOD);
  MAP(SLASH,SLASH); MAP(BACKSLASH,BACKSLASH); MAP(SEMICOLON,SEMICOLON);
  MAP(APOSTROPHE,APOSTROPHE); MAP(GRAVE,GRAVE_ACCENT);
  MAP(LEFTBRACKET,LEFT_BRACKET); MAP(RIGHTBRACKET,RIGHT_BRACKET);
  default:return PLATFORM_KEY_UNKNOWN;
  }
#undef MAP
}
static int mods(SDL_Keymod m) {
  return ((m&KMOD_SHIFT)?PLATFORM_MOD_SHIFT:0)|((m&KMOD_CTRL)?PLATFORM_MOD_CONTROL:0)|
         ((m&KMOD_ALT)?PLATFORM_MOD_ALT:0)|((m&KMOD_GUI)?PLATFORM_MOD_SUPER:0)|
         ((m&KMOD_CAPS)?PLATFORM_MOD_CAPS_LOCK:0)|((m&KMOD_NUM)?PLATFORM_MOD_NUM_LOCK:0);
}
static void append(UISDL2Input *i,InputKeyEvent e) {
  if(i->count==INPUT_KEYBOARD_CAPACITY) { i->overflow=true; return; }
  i->events[i->count++]=e;
}
void ui_sdl2_input_begin(UISDL2Input *i) {
  i->count=0; i->scroll=0; i->focus_lost=false; i->overflow=false;
}
void ui_sdl2_input_event(UISDL2Input *i,const SDL_Event *e) {
  if(e->type==SDL_WINDOWEVENT) {
    if(e->window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
      i->unfocused=true; i->down=false; i->focus_lost=true; i->count=0; i->scroll=0;
      i->mouse_head=i->mouse_count=0; i->mouse_observed_down=false;
      i->mouse_cancelled=true;
    } else if(e->window.event==SDL_WINDOWEVENT_FOCUS_GAINED) {
      i->unfocused=false;
    }
    return;
  }
  /* Keep background events from restarting a cancelled gesture. Focus loss
   * also cancels the remainder of this batch, even if focus returns in it. */
  if(i->unfocused||i->focus_lost) return;
  switch(e->type) {
  case SDL_TEXTINPUT: {
    size_t n=0; while(n<sizeof(e->text.text) && e->text.text[n]) ++n;
    for(size_t p=0;p<n;) {
      uint32_t cp; p+=utf8_decode(e->text.text+p,n-p,&cp);
      append(i,(InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint=cp});
    }
    break;
  }
  case SDL_KEYDOWN: case SDL_KEYUP:
    append(i,(InputKeyEvent){.kind=INPUT_KEY,.key=key(e->key.keysym.scancode),
      .action=e->type==SDL_KEYUP?PLATFORM_RELEASE:(e->key.repeat?PLATFORM_REPEAT:PLATFORM_PRESS),
      .mods=mods((SDL_Keymod)e->key.keysym.mod)}); break;
  case SDL_MOUSEMOTION:i->x=e->motion.x; i->y=e->motion.y; break;
  case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:
    if(e->button.button==SDL_BUTTON_LEFT) {
      bool down=e->type==SDL_MOUSEBUTTONDOWN;
      i->x=e->button.x; i->y=e->button.y;
      if(down!=i->mouse_observed_down) {
        i->mouse_observed_down=down;
        if(!i->mouse_cancelled) {
          if(i->mouse_count==UI_SDL2_MOUSE_CAPACITY) {
            i->mouse_head=i->mouse_count=0; i->down=false; i->mouse_cancelled=true;
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,"Mouse queue exceeded %d events; gestures cancelled",UI_SDL2_MOUSE_CAPACITY);
          } else {
            i->mouse_events[(i->mouse_head+i->mouse_count)%UI_SDL2_MOUSE_CAPACITY]=
              (UISDL2MouseTransition){i->x,i->y,down};
            ++i->mouse_count;
          }
        }
      }
    } break;
  case SDL_MOUSEWHEEL:
    i->scroll+=(e->wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1)*e->wheel.preciseY; break;
  }
}
bool ui_sdl2_input_frame(UISDL2Input *i,int w,int h,float dt,UIFrameInput *out) {
  if(!i||!out||i->overflow||w<=0||h<=0||!isfinite(dt)||dt<0) return false;
  *out=(UIFrameInput){.width=w,.height=h,.delta_time=dt,.mouse_x=i->x,.mouse_y=i->y,
    .mouse_down=i->down,.focus_lost=i->focus_lost||i->unfocused||i->mouse_cancelled,.scroll_delta=i->scroll,
    .events=i->events,.event_count=i->count};
  if(out->focus_lost) {
    i->mouse_head=i->mouse_count=0; i->down=false; i->mouse_cancelled=false;
    out->mouse_down=false; out->event_count=0; out->scroll_delta=0;
  } else if(i->mouse_count) {
    UISDL2MouseTransition event=i->mouse_events[i->mouse_head];
    i->mouse_head=(i->mouse_head+1)%UI_SDL2_MOUSE_CAPACITY; --i->mouse_count;
    i->down=out->mouse_down=event.down;
    out->mouse_x=event.x; out->mouse_y=event.y;
  }
  return true;
}

static bool clipboard_read(void *user,const char **text,size_t *bytes) {
  (void)user;
  char *copy=SDL_GetClipboardText();
  if(!copy) return false;
  *text=copy; *bytes=strlen(copy); return true;
}
static void clipboard_release(void *user,const char *text) { (void)user; SDL_free((void *)text); }
static bool clipboard_write(void *user,const char *text) { (void)user; return SDL_SetClipboardText(text)==0; }
bool ui_sdl2_bind_clipboard(UIContext *context) {
  const UIClipboard clipboard={clipboard_read,clipboard_release,clipboard_write};
  return ui_context_set_clipboard(context,&clipboard,NULL);
}
