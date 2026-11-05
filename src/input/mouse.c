
#include "input/mouse.h"
#include "app/application.h"

#include "platform/window.h"
#include "log/log.h"

typedef struct { double x,y; bool down; } MouseTransition;
static PlatformWindow *mouse_window;
static PlatformEventCallback previous_event;
static MouseTransition pending[INPUT_MOUSE_CAPACITY];
static size_t head,count;
static bool observed_down,displayed_down,unfocused,cancel_pending,overflow;

static void mouse_event(PlatformWindow *window,const PlatformEvent *event) {
  if(window==mouse_window) {
    if(event->kind==PLATFORM_EVENT_FOCUS) {
      unfocused=!event->value;
      if(unfocused) {
        head=count=0; observed_down=displayed_down=false; cancel_pending=true;
      }
    } else if(event->kind==PLATFORM_EVENT_BUTTON &&
              event->button==PLATFORM_MOUSE_BUTTON_LEFT && !unfocused && !cancel_pending) {
      bool down=event->action==PLATFORM_PRESS;
      if(down!=observed_down) {
        observed_down=down;
        if(count==INPUT_MOUSE_CAPACITY) {
          head=count=0; displayed_down=false; cancel_pending=overflow=true;
        } else {
          double x,y; int width,height;
          platform_cursor_position(window,&x,&y);
          platform_window_size(window,&width,&height);
          if(width>0 && height>0) {
            pending[(head+count)%INPUT_MOUSE_CAPACITY]=
              (MouseTransition){x/width,y/height,down};
            ++count;
          } else {
            head=count=0; displayed_down=false; cancel_pending=true;
          }
        }
      }
    }
  }
  if(previous_event) previous_event(window,event);
}

void input_mouse_register(PlatformWindow *window) {
  if(!window || window==mouse_window) return;
  if(mouse_window) input_mouse_unregister(mouse_window);
  mouse_window=window; head=count=0;
  unfocused=cancel_pending=overflow=false;
  observed_down=displayed_down=
    platform_mouse_button(window,PLATFORM_MOUSE_BUTTON_LEFT)==PLATFORM_PRESS;
  previous_event=platform_set_event_callback(window,mouse_event);
}
void input_mouse_unregister(PlatformWindow *window) {
  if(!window || window!=mouse_window) return;
  platform_set_event_callback(window,previous_event);
  mouse_window=NULL; previous_event=NULL; head=count=0;
  observed_down=displayed_down=unfocused=cancel_pending=overflow=false;
}

bool input_mouse_frame(APP *app,Point *position,bool *cancelled) {
  *position=get_mouse_coordinate(app);
  *cancelled=false;
  if(app->window!=mouse_window)
    return platform_mouse_button(app->window,PLATFORM_MOUSE_BUTTON_LEFT)==PLATFORM_PRESS;
  if(cancel_pending || unfocused) {
    *cancelled=true;
    if(overflow) LOG_ERROR("Mouse event queue exceeded %d events; gestures cancelled",INPUT_MOUSE_CAPACITY);
    head=count=0; displayed_down=false; cancel_pending=overflow=false;
    /* Synchronize without turning a held button into a fresh press. */
    observed_down=platform_mouse_button(app->window,PLATFORM_MOUSE_BUTTON_LEFT)==PLATFORM_PRESS;
    return false;
  }
  if(count) {
    MouseTransition event=pending[head];
    head=(head+1)%INPUT_MOUSE_CAPACITY; --count;
    position->x=event.x*VIRTUAL_W; position->y=event.y*VIRTUAL_H;
    displayed_down=event.down;
  }
  return displayed_down;
}

/* Il puntatore arriva in coordinate logiche della finestra. La conversione
 * usa quelle dimensioni, non il framebuffer HiDPI, per ottenere coordinate UI virtuali. */
Point get_mouse_coordinate(APP *app) {
  Point raw;
  platform_cursor_position(app->window, &raw.x, &raw.y);
  int win_w, win_h;
  platform_window_size(app->window, &win_w, &win_h);
  Point p = raw;
  if (win_w > 0 && win_h > 0) {
    p.x = raw.x * (double)VIRTUAL_W / (double)win_w;
    p.y = raw.y * (double)VIRTUAL_H / (double)win_h;
  }
  return p;
}
