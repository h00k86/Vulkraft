#include "view.h"
#include "backends/ui_sdl2.h"
#include "ui/draw_list.h"
#include <stdio.h>
#include <string.h>
static bool belongs(const SDL_Event *e,Uint32 id) {
  switch(e->type) {
  case SDL_KEYDOWN:case SDL_KEYUP:return e->key.windowID==id;
  case SDL_TEXTINPUT:return e->text.windowID==id;
  case SDL_MOUSEMOTION:return e->motion.windowID==id;
  case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:return e->button.windowID==id;
  case SDL_MOUSEWHEEL:return e->wheel.windowID==id;
  case SDL_WINDOWEVENT:return e->window.windowID==id;
  default:return true;
  }
}
int main(int argc,char **argv) {
  const char *root=".",*open=NULL,*dump=NULL,*font="assets/fonts/OpenSans-Italic.ttf";
  bool smoke=false,immediate=false; int result=1,frames=0;
  for(int i=1;i<argc;++i) {
    if(!strcmp(argv[i],"--root") && i+1<argc) root=argv[++i];
    else if(!strcmp(argv[i],"--open") && i+1<argc) open=argv[++i];
    else if(!strcmp(argv[i],"--font") && i+1<argc) font=argv[++i];
    else if(!strcmp(argv[i],"--dump") && i+1<argc) dump=argv[++i];
    else if(!strcmp(argv[i],"--smoke")) smoke=true;
    else if(!strcmp(argv[i],"--immediate")) immediate=true;
    else { fprintf(stderr,"Usage: %s [--root dir] [--open file] [--font font.ttf] [--immediate] [--smoke] [--dump file.bmp]\n",argv[0]); return 2; }
  }
  ConfigEditor editor; if(!config_editor_init(&editor,root)) { fprintf(stderr,"%s\n",editor.message); return 1; }
  if(open) { config_editor_request(&editor,CONFIG_ACTION_OPEN); if(!config_editor_choose(&editor,open)) { fprintf(stderr,"%s\n",editor.message); return 1; } }
  /* Keep presentation on the software path too. SDL's default window surface
   * may otherwise create an accelerated surface and replace the native window. */
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION,"0");
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0) return 1;
  SDL_Window *window=SDL_CreateWindow("Editor di configurazione",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,800,SDL_WINDOW_RESIZABLE);
  UISDL2 *backend=NULL; UIContext *context=NULL; UIDrawList *list=NULL;
  if(!window) goto done;
  SDL_SetWindowMinimumSize(window,640,480);
  backend=ui_sdl2_create(960,800,font); context=ui_context_create();
  if(!backend||!context) goto done;
  if(!immediate) { list=ui_draw_list_create(ui_sdl2_renderer(),backend); if(!list) goto done; }
  if(!ui_context_set_current(context)||!ui_context_set_renderer(context,list?ui_draw_list_renderer():ui_sdl2_renderer(),list?(void *)list:(void *)backend)||!ui_sdl2_bind_clipboard(context)) goto done;
  SDL_StartTextInput(); UISDL2Input input={.x=-1,.y=-1}; Uint64 previous=SDL_GetPerformanceCounter();
  while(!editor.close_ready && (!smoke||frames<30)) {
    ui_sdl2_input_begin(&input);
    if(smoke && frames==12) { SDL_Event close={.type=SDL_WINDOWEVENT}; close.window.windowID=SDL_GetWindowID(window); close.window.event=SDL_WINDOWEVENT_CLOSE; SDL_PushEvent(&close); }
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_QUIT) { config_editor_request(&editor,CONFIG_ACTION_CLOSE); continue; }
      if(!belongs(&event,SDL_GetWindowID(window))) continue;
      if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_CLOSE) { config_editor_request(&editor,CONFIG_ACTION_CLOSE); continue; }
      ui_sdl2_input_event(&input,&event);
    }
    if(editor.close_ready) break;
    int w,h; SDL_GetWindowSize(window,&w,&h); if(w<=0||h<=0) { SDL_Delay(10); continue; }
    Uint64 now=SDL_GetPerformanceCounter(); float dt=(float)((double)(now-previous)/SDL_GetPerformanceFrequency()); previous=now;
    UIFrameInput frame; if(!ui_sdl2_input_frame(&input,w,h,dt,&frame)) goto done;
    InputKeyEvent scripted;
    if(smoke) frame=(UIFrameInput){.width=w,.height=h,.delta_time=dt,.mouse_x=-1,.mouse_y=-1,.events=&scripted,.event_count=config_editor_smoke_event(frames,&scripted)};
    if(!ui_sdl2_resize(backend,w,h)||!ui_sdl2_begin(backend)||(list&&!ui_draw_list_begin(list))||!ui_begin_frame_input(&frame)) goto done;
    config_editor_draw(&editor,ui_sdl2_font(backend),w,h);
    ui_draw_dropdown_popups(); ui_end_frame();
    if((list&&!ui_draw_list_replay(list))||!ui_sdl2_end(backend)) goto done;
    SDL_Surface *surface=SDL_GetWindowSurface(window);
    if(!surface||SDL_BlitScaled(ui_sdl2_surface(backend),NULL,surface,NULL)<0||SDL_UpdateWindowSurface(window)<0) goto done;
    ++frames; SDL_Delay(16);
  }
  if(smoke && (frames!=30 || !config_editor_smoke_result(&editor))) { fprintf(stderr,"Editor smoke mismatch: %s focus=%s confirm=%d\n",editor.document.items[0].name,ui_keyboard_focus_id(),editor.confirm); goto done; }
  if(dump && SDL_SaveBMP(ui_sdl2_surface(backend),dump)<0) goto done;
  if(smoke) puts("Config editor SDL: PASS (text editing, dirty state, native close request, cancel preserves document)");
  result=0;
done:
  if(result) fprintf(stderr,"Editor failed: %s / %s\n",ui_sdl2_error(backend),SDL_GetError());
  if(context) ui_context_destroy(context);
  ui_draw_list_destroy(list); ui_sdl2_destroy(backend);
  SDL_StopTextInput(); SDL_DestroyWindow(window); SDL_Quit(); return result;
}
