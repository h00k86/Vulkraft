#include "backends/ui_sdl2.h"
#include "ui/draw_list.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s (%s)\n",__LINE__,#x,SDL_GetError()); return 1; } } while(0)
static uint64_t hash(SDL_Surface *s) {
  uint64_t h=14695981039346656037ULL;
  for(int y=0;y<s->h;++y) for(int x=0;x<s->w*4;++x) {
    h^=((unsigned char *)s->pixels)[y*s->pitch+x]; h*=1099511628211ULL;
  }
  return h;
}
static void mouse_event(UISDL2Input *input,bool down,int x,int y) {
  SDL_Event e={.type=down?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP};
  e.button.button=SDL_BUTTON_LEFT; e.button.x=x; e.button.y=y;
  ui_sdl2_input_event(input,&e);
}
static int mouse_frame(UISDL2Input *input,UISDL2 *backend,bool down,bool cancelled,
                       float x,float y,bool clicked) {
  UIFrameInput frame;
  CHECK(ui_sdl2_input_frame(input,480,240,.016f,&frame));
  CHECK(frame.mouse_down==down && frame.focus_lost==cancelled);
  CHECK(frame.mouse_x==x && frame.mouse_y==y);
  CHECK(ui_sdl2_begin(backend)&&ui_begin_frame_input(&frame));
  CHECK(ui_button_labeled("mouse/test","Click",ui_sdl2_font(backend),
                         ANCHOR_TOP_LEFT,10,1,0,0,NULL)==clicked);
  ui_end_frame(); CHECK(ui_sdl2_end(backend));
  ui_sdl2_input_begin(input);
  return 0;
}
static void scene(const UIRenderer *r,void *u,Font *font) {
  r->draw_quad(u,0,0,80,80,1,0,0);
  r->text_clip_push(u,10,10,20,20);
  r->draw_quad(u,0,0,80,80,0,1,0);
  r->text_clip_push(u,100,100,10,10); /* Empty intersection must draw nothing. */
  r->draw_quad(u,0,0,80,80,0,0,1);
  r->text_clip_pop(u); r->text_clip_pop(u);
  r->draw_quad_alpha(u,40,40,20,20,0,0,1,.5f);
  r->draw_text_colored(u,font,"Caffè € — AV",10,110,24,1,1,1);
  r->draw_textured_quad(u,160,10,32,32,0,0,0,1,1);
}
int main(void) {
  CHECK(SDL_Init(SDL_INIT_VIDEO)==0);
  CHECK(!ui_sdl2_create(100,100,"/missing-font.ttf"));
  UISDL2 *b=ui_sdl2_create(320,200,"assets/fonts/OpenSans-Italic.ttf"); CHECK(b);
  const UIRenderer *r=ui_sdl2_renderer(); Font *font=ui_sdl2_font(b);
  SDL_Surface *fixture=SDL_CreateRGBSurfaceWithFormat(0,4,4,32,SDL_PIXELFORMAT_RGBA32); CHECK(fixture);
  SDL_FillRect(fixture,NULL,SDL_MapRGBA(fixture->format,255,255,0,255));
  char path[128]; snprintf(path,sizeof(path),"/tmp/ui-sdl2-%ld.bmp",(long)getpid());
  CHECK(SDL_SaveBMP(fixture,path)==0); SDL_FreeSurface(fixture);
  CHECK(r->texture_manager_preview_load(b,path)==0);
  int w=0,h=0; CHECK(r->texture_manager_get_size(b,0,&w,&h)&&w==4&&h==4);
  CHECK(ui_sdl2_begin(b));
  CHECK(r->measure_text_width(b,font,"Caffè €",24)>40 && r->font_ascent(b,font,24)>10);
  scene(r,b,font); CHECK(ui_sdl2_end(b));
  SDL_Surface *s=ui_sdl2_surface(b);
  Uint32 *row=(Uint32 *)((char *)s->pixels+15*s->pitch); Uint8 red,green,blue,alpha;
  SDL_GetRGBA(row[15],s->format,&red,&green,&blue,&alpha); CHECK(green==255&&red==0&&blue==0);
  SDL_GetRGBA(row[5],s->format,&red,&green,&blue,&alpha); CHECK(red==255&&green==0);
  row=(Uint32 *)((char *)s->pixels+50*s->pitch);
  SDL_GetRGBA(row[50],s->format,&red,&green,&blue,&alpha);
  CHECK(red>=120&&red<=135&&blue>=120&&blue<=135&&green==0);
  unsigned ink=0;
  for(int y=85;y<112;++y) for(int x=10;x<150;++x) {
    Uint32 pixel=*(Uint32 *)((char *)s->pixels+y*s->pitch+x*4);
    SDL_GetRGBA(pixel,s->format,&red,&green,&blue,&alpha); if(red>100&&green>100) ++ink;
  }
  CHECK(ink>20); uint64_t immediate=hash(s);
  UIDrawList *list=ui_draw_list_create(r,b); CHECK(list);
  CHECK(ui_sdl2_begin(b)&&ui_draw_list_begin(list));
  scene(ui_draw_list_renderer(),list,font);
  CHECK(ui_draw_list_replay(list)&&ui_sdl2_end(b)&&hash(s)==immediate);
  CHECK(!ui_sdl2_resize(b,0,240) && ui_sdl2_surface(b)==s);
  CHECK(r->texture_manager_preview_load(b,"/missing-preview.png")==-1);
  CHECK(r->texture_manager_get_size(b,0,&w,&h)&&w==4&&h==4);
  CHECK(ui_sdl2_resize(b,480,240));
  CHECK(ui_sdl2_begin(b)); scene(r,b,font); CHECK(ui_sdl2_end(b));
  CHECK(r->texture_manager_get_size(b,0,&w,&h)&&w==4&&h==4);
  CHECK(ui_sdl2_begin(b));
  for(int i=0;i<32;++i) CHECK(r->text_clip_push(b,0,0,20,20));
  CHECK(!r->text_clip_push(b,0,0,20,20)&&r->text_clip_depth(b)==32);
  for(int i=0;i<32;++i) CHECK(r->text_clip_pop(b));
  CHECK(!r->text_clip_pop(b)&&r->text_clip_depth(b)==0&&ui_sdl2_end(b));
  /* Invalid lifecycle calls preserve the in-progress image and its resources. */
  CHECK(!ui_sdl2_end(b));
  CHECK(ui_sdl2_begin(b));
  r->draw_quad(b,0,0,40,40,1,0,0);
  CHECK(!ui_sdl2_begin(b));
  s=ui_sdl2_surface(b);
  CHECK(!ui_sdl2_resize(b,480,240)&&!ui_sdl2_resize(b,320,200));
  CHECK(ui_sdl2_surface(b)==s&&ui_sdl2_end(b));
  SDL_GetRGBA(*(Uint32 *)s->pixels,s->format,&red,&green,&blue,&alpha);
  CHECK(red==255&&green==0&&blue==0);
  uint64_t finished=hash(s);
  CHECK(!ui_sdl2_end(b)&&hash(s)==finished);
  /* Unbalanced clips can be repaired, or explicitly abandoned. */
  CHECK(ui_sdl2_begin(b)&&r->text_clip_push(b,0,0,20,20));
  CHECK(!ui_sdl2_end(b)&&!ui_sdl2_begin(b));
  CHECK(r->text_clip_pop(b)&&ui_sdl2_end(b));
  CHECK(ui_sdl2_begin(b)&&r->text_clip_push(b,0,0,20,20));
  r->draw_quad(b,NAN,0,10,10,1,1,1);
  CHECK(ui_sdl2_error(b)[0]&&!ui_sdl2_end(b));
  char error[192]; snprintf(error,sizeof(error),"%s",ui_sdl2_error(b));
  ui_sdl2_discard(b);
  CHECK(r->text_clip_depth(b)==0&&!strcmp(error,ui_sdl2_error(b)));
  ui_sdl2_discard(b); ui_sdl2_discard(NULL);
  CHECK(ui_sdl2_resize(b,480,240)&&ui_sdl2_begin(b));
  CHECK(!ui_sdl2_error(b)[0]);
  r->draw_quad(b,100,100,10,10,0,1,0);
  CHECK(ui_sdl2_end(b));
  s=ui_sdl2_surface(b);
  row=(Uint32 *)((char *)s->pixels+105*s->pitch);
  SDL_GetRGBA(row[105],s->format,&red,&green,&blue,&alpha);
  CHECK(green==255&&red==0&&blue==0); /* Discard removed the old clip. */
  /* Recover both layers after a replay reaches a backend drawing error. */
  CHECK(ui_sdl2_begin(b)&&ui_draw_list_begin(list));
  ui_draw_list_renderer()->draw_quad(list,NAN,0,10,10,1,1,1);
  CHECK(ui_draw_list_replay(list)); /* Void drawing callbacks cannot report errors. */
  CHECK(!ui_sdl2_end(b)&&ui_sdl2_error(b)[0]);
  ui_sdl2_discard(b);
  CHECK(ui_draw_list_discard(list)&&ui_sdl2_resize(b,480,240));
  CHECK(ui_sdl2_begin(b)&&ui_draw_list_begin(list));
  scene(ui_draw_list_renderer(),list,font);
  CHECK(ui_draw_list_replay(list)&&ui_sdl2_end(b));
  /* Exercise the SDL-to-UI adapter with real widgets, no native input injection. */
  UISDL2Input input={0}; SDL_Event event={.type=SDL_TEXTINPUT};
  snprintf(event.text.text,sizeof(event.text.text),"Caffè €");
  ui_sdl2_input_begin(&input); ui_sdl2_input_event(&input,&event); CHECK(input.count==7);
  UIContext *ctx=ui_context_create(); CHECK(ctx&&ui_context_set_current(ctx));
  CHECK(ui_context_set_renderer(ctx,r,b)); ui_text_input_focus("text");
  UIFrameInput frame; CHECK(ui_sdl2_input_frame(&input,480,240,.016f,&frame));
  CHECK(ui_sdl2_begin(b)&&ui_begin_frame_input(&frame));
  char value[64]="";
  ui_text_input("text",font,ANCHOR_TOP_LEFT,0,18,10,10,250,value,sizeof(value),NULL);
  ui_end_frame(); CHECK(ui_sdl2_end(b)&&!strcmp(value,"Caffè €"));
  CHECK(ui_sdl2_bind_clipboard(ctx));
  const SDL_Scancode edit_keys[]={SDL_SCANCODE_A,SDL_SCANCODE_C,SDL_SCANCODE_X,
                                SDL_SCANCODE_V,SDL_SCANCODE_Z,SDL_SCANCODE_Y};
  const char *edit_expected[]={"Caffè €","Caffè €","","Caffè €","","Caffè €"};
  for(size_t i=0;i<sizeof(edit_keys)/sizeof(*edit_keys);++i) {
    ui_sdl2_input_begin(&input);
    event=(SDL_Event){.type=SDL_KEYDOWN}; event.key.keysym.scancode=edit_keys[i]; event.key.keysym.mod=KMOD_CTRL;
    ui_sdl2_input_event(&input,&event);
    CHECK(ui_sdl2_input_frame(&input,480,240,.016f,&frame));
    CHECK(ui_sdl2_begin(b)&&ui_begin_frame_input(&frame));
    ui_text_input("text",font,ANCHOR_TOP_LEFT,0,18,10,10,250,value,sizeof(value),NULL);
    ui_end_frame(); CHECK(ui_sdl2_end(b)&&!strcmp(value,edit_expected[i]));
    if(i==1) { char *copied=SDL_GetClipboardText(); CHECK(copied&&!strcmp(copied,"Caffè €")); SDL_free(copied); }
  }
  ui_sdl2_input_begin(&input);
  event=(SDL_Event){.type=SDL_KEYDOWN}; event.key.keysym.scancode=SDL_SCANCODE_LEFT;
  event.key.keysym.mod=KMOD_CTRL; event.key.repeat=1;
  ui_sdl2_input_event(&input,&event);
  CHECK(input.events[0].key==PLATFORM_KEY_LEFT&&input.events[0].action==PLATFORM_REPEAT&&
        input.events[0].mods==PLATFORM_MOD_CONTROL);
  event=(SDL_Event){.type=SDL_WINDOWEVENT}; event.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
  input.down=true; ui_sdl2_input_event(&input,&event);
  CHECK(input.focus_lost&&!input.down&&!input.count);
  event=(SDL_Event){.type=SDL_MOUSEBUTTONDOWN}; event.button.button=SDL_BUTTON_LEFT;
  ui_sdl2_input_event(&input,&event); CHECK(!input.down);
  ui_sdl2_input_begin(&input);
  event=(SDL_Event){.type=SDL_TEXTINPUT}; strcpy(event.text.text,"background");
  ui_sdl2_input_event(&input,&event); CHECK(!input.count);
  event=(SDL_Event){.type=SDL_MOUSEWHEEL}; event.wheel.preciseY=2;
  ui_sdl2_input_event(&input,&event); CHECK(input.scroll==0);
  event=(SDL_Event){.type=SDL_WINDOWEVENT}; event.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;
  ui_sdl2_input_event(&input,&event);
  event=(SDL_Event){.type=SDL_TEXTINPUT}; strcpy(event.text.text,"x");
  ui_sdl2_input_event(&input,&event); CHECK(input.count==1);
  event=(SDL_Event){.type=SDL_WINDOWEVENT}; event.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
  ui_sdl2_input_event(&input,&event);
  event.window.event=SDL_WINDOWEVENT_FOCUS_GAINED; ui_sdl2_input_event(&input,&event);
  event=(SDL_Event){.type=SDL_TEXTINPUT}; strcpy(event.text.text,"x");
  ui_sdl2_input_event(&input,&event); CHECK(!input.count&&input.focus_lost);
  ui_sdl2_input_begin(&input); event=(SDL_Event){.type=SDL_TEXTINPUT}; strcpy(event.text.text,"x");
  for(int n=0;n<=INPUT_KEYBOARD_CAPACITY;++n) ui_sdl2_input_event(&input,&event);
  CHECK(input.overflow&&!ui_sdl2_input_frame(&input,480,240,.016f,&frame));
  input=(UISDL2Input){0};
  ui_release_input_capture();
  /* The host drains all events. Two taps and later motion must retain four
   * transitions and their original coordinates across subsequent begins. */
  for(int tap=0;tap<2;++tap) {
    mouse_event(&input,true,11,11); mouse_event(&input,false,11,11);
  }
  event=(SDL_Event){.type=SDL_MOUSEMOTION}; event.motion.x=400; event.motion.y=200;
  ui_sdl2_input_event(&input,&event);
  UIFrameInput invalid;
  CHECK(!ui_sdl2_input_frame(&input,0,240,.016f,&invalid)&&input.mouse_count==4);
  for(int tap=0;tap<2;++tap) {
    CHECK(!mouse_frame(&input,b,true,false,11,11,false));
    CHECK(!mouse_frame(&input,b,false,false,11,11,true));
  }
  CHECK(!mouse_frame(&input,b,false,false,400,200,false));
  mouse_event(&input,true,11,11); mouse_event(&input,true,11,11); /* Duplicate. */
  mouse_event(&input,false,400,200);
  CHECK(input.mouse_count==2);
  CHECK(!mouse_frame(&input,b,true,false,11,11,false));
  CHECK(!mouse_frame(&input,b,false,false,400,200,false));
  mouse_event(&input,true,11,11);
  CHECK(!mouse_frame(&input,b,true,false,11,11,false));
  mouse_event(&input,false,11,11);
  event=(SDL_Event){.type=SDL_WINDOWEVENT}; event.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
  ui_sdl2_input_event(&input,&event);
  event.window.event=SDL_WINDOWEVENT_FOCUS_GAINED; ui_sdl2_input_event(&input,&event);
  mouse_event(&input,true,11,11); mouse_event(&input,false,11,11);
  CHECK(!mouse_frame(&input,b,false,true,11,11,false));
  CHECK(!mouse_frame(&input,b,false,false,11,11,false));
  mouse_event(&input,true,11,11);
  CHECK(!mouse_frame(&input,b,true,false,11,11,false));
  for(int n=0;n<UI_SDL2_MOUSE_CAPACITY+2;++n) mouse_event(&input,n%2,11,11);
  CHECK(input.mouse_cancelled && !input.mouse_count);
  CHECK(!mouse_frame(&input,b,false,true,11,11,false));
  CHECK(!mouse_frame(&input,b,false,false,11,11,false));
  mouse_event(&input,false,11,11);
  CHECK(!mouse_frame(&input,b,false,false,11,11,false));
  mouse_event(&input,true,11,11); mouse_event(&input,false,11,11);
  CHECK(!mouse_frame(&input,b,true,false,11,11,false));
  CHECK(!mouse_frame(&input,b,false,false,11,11,true));
  /* Regression: subpixel-height scan strips vanished in the software backend. */
  frame=(UIFrameInput){.width=480,.height=240,.mouse_x=-1,.mouse_y=-1};
  CHECK(ui_sdl2_begin(b)&&ui_begin_frame_input(&frame));
  int radio=0;
  ui_radio_button("radio/selected",font,(UIRect){20,20,180,30},"Selected",0,&radio,false);
  ui_radio_button("radio/empty",font,(UIRect){20,60,180,30},"Empty",1,&radio,false);
  ui_end_frame(); CHECK(ui_sdl2_end(b));
  s=ui_sdl2_surface(b);
  Uint32 selected_pixel=*(Uint32 *)((char *)s->pixels+35*s->pitch+30*4);
  Uint32 empty_pixel=*(Uint32 *)((char *)s->pixels+75*s->pitch+30*4);
  CHECK(selected_pixel!=empty_pixel);
  CHECK(ui_context_destroy(ctx)); ui_draw_list_destroy(list); ui_sdl2_destroy(b);
  CHECK(unlink(path)==0); SDL_Quit();
  puts("SDL2 software: PASS (pixels, alpha, nested/empty clips, UTF-8 glyphs/input, clipboard/selection/undo, image, resize, frame lifecycle/recovery, immediate/deferred equality)");
  return 0;
}
