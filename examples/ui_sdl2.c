#include "backends/ui_sdl2.h"
#include "ui/draw_list.h"
#include "ui_reference_scene.h"
#include <stdio.h>
#include <stdlib.h>
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
  int limit=0,result=1;
  bool deferred=true,smoke=false,navigation=false,editing=false,layout=false,color=false,basic=false;
  const char *dump=NULL,*font_path="assets/fonts/OpenSans-Italic.ttf",*root=".";
  for(int i=1;i<argc;++i) {
    if(!strcmp(argv[i],"--immediate")) deferred=false;
    else if(!strcmp(argv[i],"--smoke")) { smoke=true; limit=30; }
    else if(!strcmp(argv[i],"--navigation-smoke")) { navigation=true; limit=30; }
    else if(!strcmp(argv[i],"--basic-smoke")) { basic=true; limit=30; }
    else if(!strcmp(argv[i],"--color-smoke")) { color=true; limit=30; }
    else if(!strcmp(argv[i],"--layout-smoke")) { layout=true; limit=30; }
    else if(!strcmp(argv[i],"--editing-smoke")) { editing=true; limit=30; }
    else if(!strcmp(argv[i],"--frames")&&i+1<argc) limit=atoi(argv[++i]);
    else if(!strcmp(argv[i],"--dump")&&i+1<argc) dump=argv[++i];
    else if(!strcmp(argv[i],"--font")&&i+1<argc) font_path=argv[++i];
    else if(!strcmp(argv[i],"--root")&&i+1<argc) root=argv[++i];
    else { fprintf(stderr,"Usage: %s [--immediate] [--smoke | --navigation-smoke | --editing-smoke | --layout-smoke | --color-smoke | --basic-smoke] [--frames N] [--dump file.bmp] [--font font.ttf] [--root directory]\n",argv[0]); return 2; }
  }
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION,"0");
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0) { fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
  SDL_Window *window=SDL_CreateWindow("UI reference — SDL software",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                                     960,720,SDL_WINDOW_RESIZABLE);
  UISDL2 *backend=NULL; UIContext *context=NULL; UIDrawList *list=NULL;
  if(!window) goto done;
  backend=ui_sdl2_create(960,720,font_path); context=ui_context_create();
  if(!backend||!context) goto done;
  if(deferred) { list=ui_draw_list_create(ui_sdl2_renderer(),backend); if(!list) goto done; }
  if(!ui_context_set_current(context)||!ui_context_set_renderer(context,
        list?ui_draw_list_renderer():ui_sdl2_renderer(),list?(void *)list:(void *)backend)) goto done;
  SDL_StartTextInput();
  if(!ui_sdl2_bind_clipboard(context)) goto done;
  UIReferenceScene scene; ui_reference_init(&scene); scene.layout_demo=layout; scene.options=color; scene.basic_dialog=basic;
  if(navigation||editing) scene.text[0]=0;
  UISDL2Input input={.x=-1,.y=-1};
  Uint64 previous=SDL_GetPerformanceCounter(); bool running=true; int frames=0;
  while(running && (!limit||frames<limit)) {
    ui_sdl2_input_begin(&input);
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      if(event.type==SDL_QUIT) { running=false; break; }
      if(!belongs(&event,SDL_GetWindowID(window))) continue;
      if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_CLOSE) { running=false; break; }
      ui_sdl2_input_event(&input,&event);
    }
    if(!running) break;
    if(smoke) {
      if(frames==0) { scene.text[0]=0; ui_text_input_focus("reference/text"); }
      if(frames==1) {
        event=(SDL_Event){.type=SDL_TEXTINPUT}; strcpy(event.text.text,"Caffè €");
        ui_sdl2_input_event(&input,&event);
      }
      if(frames==10) SDL_SetWindowSize(window,800,600);
      if(frames==20) SDL_SetWindowSize(window,960,720);
    }
    int w,h; SDL_GetWindowSize(window,&w,&h);
    if(w<=0||h<=0) { SDL_Delay(10); continue; }
    Uint64 now=SDL_GetPerformanceCounter(); float dt=(float)((double)(now-previous)/SDL_GetPerformanceFrequency()); previous=now;
    UIFrameInput frame;
    if(!ui_sdl2_input_frame(&input,w,h,dt,&frame)) goto done;
    InputKeyEvent scripted;
    if(navigation||editing||layout||color||basic) {
      frame=(UIFrameInput){.width=w,.height=h,.delta_time=dt,.mouse_x=-1,.mouse_y=-1,
        .events=&scripted,.event_count=basic?ui_reference_basic_events(frames,&scripted):color?ui_reference_color_events(frames,&scripted):layout?ui_reference_layout_events(frames,&scripted):editing?ui_reference_editing_events(frames,&scripted):ui_reference_navigation_events(frames,&scripted)};
    }
    if(!ui_sdl2_resize(backend,w,h)||
       !ui_sdl2_begin(backend)||(list&&!ui_draw_list_begin(list))||!ui_begin_frame_input(&frame)) goto done;
    ui_reference_draw(&scene,ui_sdl2_font(backend),w,h,root);
    ui_draw_dropdown_popups(); ui_end_frame();
    if((list&&!ui_draw_list_replay(list))||!ui_sdl2_end(backend)) goto done;
    SDL_Surface *surface=SDL_GetWindowSurface(window);
    if(!surface||SDL_BlitScaled(ui_sdl2_surface(backend),NULL,surface,NULL)<0||SDL_UpdateWindowSurface(window)<0) goto done;
    ++frames; SDL_Delay(16);
  }
  if(smoke && (frames!=30||strcmp(scene.text,"Caffè €"))) { fprintf(stderr,"Smoke input mismatch: %s\n",scene.text); goto done; }
  if(navigation && (frames!=30 || !ui_reference_navigation_result(&scene))) {
    fprintf(stderr,"Navigation mismatch: focus=%s text=%s checked=%d value=%g choice=%d clicks=%d dialog=%d\n",
      ui_keyboard_focus_id(),scene.text,scene.checked,scene.value,scene.choice,scene.clicks,scene.dialog); goto done;
  }
  if(editing && (frames!=30 || !ui_reference_editing_result(&scene))) { fprintf(stderr,"Editing smoke mismatch\n"); goto done; }
  if(layout && !ui_reference_layout_result(&scene)) { fprintf(stderr,"Layout smoke mismatch\n"); goto done; }
  if(color && !ui_reference_color_result(&scene)) { fprintf(stderr,"Color smoke mismatch\n"); goto done; }
  if(basic && !ui_reference_basic_result(&scene)) { fprintf(stderr,"Basic widgets smoke mismatch\n"); goto done; }
  if(dump && SDL_SaveBMP(ui_sdl2_surface(backend),dump)<0) goto done;
  printf("SDL software reference: PASS frames=%d mode=%s text=%s\n",frames,deferred?"deferred":"immediate",scene.text);
  result=0;
done:
  if(result) fprintf(stderr,"SDL reference failed: backend=%s SDL=%s\n",ui_sdl2_error(backend),SDL_GetError());
  if(context) ui_context_destroy(context);
  ui_draw_list_destroy(list); ui_sdl2_destroy(backend);
  SDL_StopTextInput(); SDL_DestroyWindow(window); SDL_Quit(); return result;
}
