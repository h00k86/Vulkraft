/* Public API integration: picker and core are separate translation units. */
#define _POSIX_C_SOURCE 200809L
#include "ui/ui.h"
#include "ui/file_picker.h"
/* Backend-owned font fixture; no concrete renderer dependency. */
struct Font { float ascender; };
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static bool fail_picker_allocation;
static int clip_limit=100;
static bool draw_source;
static char source_text[512]="abcdef";
void *__real_calloc(size_t count,size_t size);
void *__wrap_calloc(size_t count,size_t size) {
  return fail_picker_allocation ? NULL : __real_calloc(count,size);
}
typedef struct { int depth, draws; char find[64]; float x,y; bool found; } Capture;
static void test_draw_quad(void *user, float x, float y, float w, float h, float r, float g, float b) {
  Capture *capture=user; (void)capture;

}
static void test_draw_quad_alpha(void *user, float x, float y, float w, float h, float r, float g, float b, float a) {
  Capture *capture=user; (void)capture;

}
static void test_draw_text_colored(void *user, Font *font, const char *text, float x, float y, float scale, float r, float g, float b) {
  Capture *capture=user; (void)capture;
  ++capture->draws;
  if (capture->find[0] && !strcmp(text,capture->find)) {
    capture->found=true; capture->x=x+2; capture->y=y-2;
  }
}
static void test_draw_textured_quad(void *user, float x, float y, float w, float h, int textureSlot, float u0, float v0, float u1, float v1) {
  Capture *capture=user; (void)capture;

}
static void test_draw_hud_paper_panel(void *user, float x, float y, float w, float h, const float fill[4], const float accent[3], bool focused, uint32_t seed) {
  Capture *capture=user; (void)capture;

}
static void test_draw_hud_trash_swipe(void *user, float x, float y, float w, float h, bool focused, uint32_t seed, int textureSlot) {
  Capture *capture=user; (void)capture;

}
static float test_font_ascent(void *user, Font *font, float scale) {
  return font->ascender * scale;
}
static float test_measure_text_width(void *user, Font *font, const char *text, float scale) {
  Capture *capture=user; (void)capture;
  return (float)strlen(text)*scale*0.5f;
}
static bool test_text_clip_push(void *user, float x, float y, float w, float h) {
  Capture *capture=user; (void)capture;
  if (capture->depth>=clip_limit) return false;
  ++capture->depth; return true;
}
static bool test_text_clip_pop(void *user) {
  Capture *capture=user; (void)capture;
  if (!capture->depth) return false;
  --capture->depth; return true;
}
static int test_text_clip_depth(void *user) {
  Capture *capture=user; (void)capture;
  return capture->depth;
}
static bool test_texture_manager_get_size(void *user, int slot, int *outW, int *outH) {
  Capture *capture=user; (void)capture;
  return false;
}
static int test_texture_manager_preview_load(void *user, const char *path) {
  Capture *capture=user; (void)capture;
  return -1;
}
static void test_texture_manager_preview_clear(void *user) {
  Capture *capture=user; (void)capture;

}
static const UIRenderer backend={
  .draw_quad=test_draw_quad,
  .draw_quad_alpha=test_draw_quad_alpha,
  .draw_text_colored=test_draw_text_colored,
  .draw_textured_quad=test_draw_textured_quad,
  .draw_hud_paper_panel=test_draw_hud_paper_panel,
  .draw_hud_trash_swipe=test_draw_hud_trash_swipe,
  .measure_text_width=test_measure_text_width,
    .font_ascent=test_font_ascent,
  .text_clip_push=test_text_clip_push,
  .text_clip_pop=test_text_clip_pop,
  .text_clip_depth=test_text_clip_depth,
  .texture_manager_get_size=test_texture_manager_get_size,
  .texture_manager_preview_load=test_texture_manager_preview_load,
  .texture_manager_preview_clear=test_texture_manager_preview_clear,
};
static bool frame(Capture *capture, bool down, float x, float y, char *path, size_t capacity) {
  UIFrameInput input={.width=1280,.height=800,.delta_time=0.016f,
                     .mouse_x=x,.mouse_y=y,.mouse_down=down};
  Font font={.ascender=1};
  assert(ui_begin_frame_input(&input));
  if(draw_source) {
    UITextEditorOptions options={.scale=14};
    ui_text_editor("source",&font,(UIRect){20,20,280,80},source_text,sizeof(source_text),&options);
  }
  bool selected=ui_file_picker_draw(&font,path,capacity);
  ui_draw_dropdown_popups(); ui_end_frame();
  assert(capture->depth==0);
  return selected;
}
static bool click(Capture *capture,const char *label,char *path,size_t capacity) {
  ui_humanize_label(label,capture->find,sizeof(capture->find)); capture->found=false;
  assert(!frame(capture,false,-1,-1,path,capacity));
  assert(capture->found);
  float x=capture->x,y=capture->y;
  assert(!frame(capture,true,x,y,path,capacity));
  return frame(capture,false,x,y,path,capacity);
}
static bool visible(Capture *capture,const char *label,char *out,size_t capacity) {
  snprintf(capture->find,sizeof(capture->find),"%s",label); capture->found=false;
  assert(!frame(capture,false,-1,-1,out,capacity)); return capture->found;
}
static void multiple_extensions(const char *root,Capture *capture,char *out,size_t capacity) {
  const char *names[]={"code.vert","code.frag","code.c","code.bin","UP.VERT"};
  char path[512];
  for(size_t i=0;i<sizeof(names)/sizeof(*names);++i) {
    snprintf(path,sizeof(path),"%s/%s",root,names[i]); FILE *f=fopen(path,"w"); assert(f); fclose(f);
  }
  snprintf(path,sizeof(path),"%s/folder",root); assert(mkdir(path,0700)==0);
  char first[24]=".vert"; const char *extensions[]={first,".frag",".c"};
  UIFilePickerRequest request={.mode=UI_FILE_PICKER_OPEN,.root_directory=root,
      .extension=".txt",.extensions=extensions,.extension_count=3};
  ui_file_picker_open("multi","Multiple extensions",&request);
  strcpy(first,".junk"); /* All suffixes are copied, not borrowed across frames. */
  assert(visible(capture,"   code.vert",out,capacity));
  assert(visible(capture,"   code.frag",out,capacity));
  assert(visible(capture,"   code.c",out,capacity));
  assert(visible(capture,">  folder",out,capacity));
  assert(!visible(capture,"   inside.txt",out,capacity));
  assert(!visible(capture,"   code.bin",out,capacity));
  assert(!visible(capture,"   UP.VERT",out,capacity)); /* Case-sensitive by contract. */
  assert(!click(capture,"   code.frag",out,capacity));
  assert(click(capture,"Apri",out,capacity));
  snprintf(path,sizeof(path),"%s/code.frag",root); assert(!strcmp(out,path));
  strcpy(first,".vert"); request.mode=UI_FILE_PICKER_SAVE; request.initial_name="new.frag";
  ui_file_picker_open("multi","Save accepted suffix",&request);
  assert(click(capture,"Salva",out,capacity));
  snprintf(path,sizeof(path),"%s/new.frag",root); assert(!strcmp(out,path)&&access(path,F_OK)!=0);
  request.initial_name="new";
  ui_file_picker_open("multi","No implicit first suffix",&request);
  assert(!click(capture,"Salva",out,capacity)&&ui_file_picker_is_open("multi"));
  ui_file_picker_cancel();
  request.default_extension=".vert";
  ui_file_picker_open("multi","Explicit default",&request);
  assert(click(capture,"Salva",out,capacity));
  snprintf(path,sizeof(path),"%s/new.vert",root); assert(!strcmp(out,path));
  request.initial_name="new.bin";
  ui_file_picker_open("multi","Reject explicit wrong suffix",&request);
  assert(!click(capture,"Salva",out,capacity)&&ui_file_picker_is_open("multi"));
  /* Invalid requests preserve an active picker, never broaden a filter. */
  request.default_extension=".txt";
  ui_file_picker_open("invalid","Invalid default",&request);
  assert(ui_file_picker_is_open("multi")&&!ui_file_picker_is_open("invalid"));
  request.default_extension=NULL; request.extension_count=17;
  ui_file_picker_open("invalid","Too many",&request); assert(ui_file_picker_is_open("multi"));
  request.extension_count=1; request.extensions=NULL;
  ui_file_picker_open("invalid","NULL list",&request); assert(ui_file_picker_is_open("multi"));
  const char *bad[]={"*.vert"}; request.extensions=bad;
  ui_file_picker_open("invalid","No globs",&request); assert(ui_file_picker_is_open("multi"));
  bad[0]=".really_really_long_extension";
  ui_file_picker_open("invalid","No truncated suffix",&request); assert(ui_file_picker_is_open("multi"));
  bad[0]=NULL;
  ui_file_picker_open("invalid","NULL suffix",&request); assert(ui_file_picker_is_open("multi"));
  ui_file_picker_cancel();
  /* A zero count continues to use the old single extension. */
  request=(UIFilePickerRequest){.root_directory=root,.extension=".txt"};
  ui_file_picker_open("legacy","Legacy",&request);
  assert(visible(capture,"   inside.txt",out,capacity)&&!visible(capture,"   code.vert",out,capacity));
  ui_file_picker_cancel();
  for(size_t i=0;i<sizeof(names)/sizeof(*names);++i) {
    snprintf(path,sizeof(path),"%s/%s",root,names[i]); assert(unlink(path)==0);
  }
  snprintf(path,sizeof(path),"%s/folder",root); assert(rmdir(path)==0);
}
static void picker_return_focus(const char *root,Capture *capture,char *out,size_t capacity) {
  draw_source=true;
  assert(!frame(capture,false,-1,-1,out,capacity));
  ui_text_input_focus("source"); assert(!frame(capture,false,-1,-1,out,capacity));
  /* Simulate a toolbar/menu taking focus before opening the picker. */
  ui_release_input_capture();
  UIFilePickerRequest request={.mode=UI_FILE_PICKER_OPEN,.root_directory=root,
    .extension=".txt",.return_focus="source",.resume_editing=true};
  ui_file_picker_open("return-picker","Return to source",&request);
  assert(!frame(capture,false,-1,-1,out,capacity));
  assert(!ui_text_input_is_focused("source"));
  assert(!click(capture,"   inside.txt",out,capacity));
  assert(click(capture,"Apri",out,capacity));
  assert(ui_text_input_is_focused("source")&&ui_has_keyboard_capture());
  assert(!strcmp(source_text,"abcdef"));
  /* Programmatic cancellation takes the identical restore path. */
  ui_file_picker_open("return-picker","Cancel",&request);
  assert(!frame(capture,false,-1,-1,out,capacity)); ui_file_picker_cancel();
  assert(ui_text_input_is_focused("source"));
  /* Cancelling overwrite returns to the still-open picker, not the source. */
  request.mode=UI_FILE_PICKER_SAVE; request.initial_name="inside.txt";
  ui_file_picker_open("return-picker","Overwrite nesting",&request);
  assert(!click(capture,"Salva",out,capacity));
  assert(!ui_text_input_is_focused("source"));
  assert(!click(capture,"Annulla",out,capacity));
  assert(ui_file_picker_is_open("return-picker")&&!ui_text_input_is_focused("source"));
  assert(!frame(capture,false,-1,-1,out,capacity)&&!ui_text_input_is_focused("source"));
  assert(!click(capture,"Salva",out,capacity));
  assert(click(capture,"Sovrascrivi",out,capacity));
  assert(!ui_file_picker_is_open("return-picker")&&ui_text_input_is_focused("source"));
  draw_source=false; ui_release_input_capture();
}
int main(void) {
  char root[]="/tmp/ui-picker-test-XXXXXX"; assert(mkdtemp(root));
  char file[512]; snprintf(file,sizeof(file),"%s/inside.txt",root);
  FILE *f=fopen(file,"w"); assert(f); fputs("fixture",f); fclose(f);
  UIContext *a=ui_context_create(), *b=ui_context_create();
  Capture ca={0},cb={0};
  assert(a && b && ui_context_set_renderer(a,&backend,&ca) && ui_context_set_renderer(b,&backend,&cb));
  assert(ui_context_set_current(a));
  char out[512]="unchanged";
  assert(!ui_file_picker_is_open("picker")); ui_file_picker_cancel();
  assert(!ui_file_picker_selected_path("picker",out,sizeof(out)) && !strcmp(out,"unchanged"));
  UIFilePickerRequest request={.mode=UI_FILE_PICKER_DIRECTORY,.root_directory=root};
  fail_picker_allocation=true;
  ui_file_picker_open("oom","Allocation failure",&request);
  assert(!ui_file_picker_is_open("oom") && !ui_has_input_capture());
  fail_picker_allocation=false;
  ui_file_picker_open("picker","Directory",&request);
  for (clip_limit=0;clip_limit<2;++clip_limit) {
    snprintf(ca.find,sizeof(ca.find),"Seleziona cartella"); ca.found=false;
    assert(!frame(&ca,false,-1,-1,out,sizeof(out)));
    assert(!ca.found && ui_file_picker_is_open("picker"));
  }
  clip_limit=100;
  UIInputDiagnostics diagnostics;
  assert(ui_get_input_diagnostics(&diagnostics) && diagnostics.file_picker_open);
  assert(ui_has_pointer_capture() && ui_has_keyboard_capture());
  assert(click(&ca,"Seleziona cartella",out,sizeof(out)) && !strcmp(out,root));
  assert(!ui_file_picker_is_open("picker") && !ui_has_input_capture());
  request.mode=UI_FILE_PICKER_SAVE; request.initial_name="new"; request.extension=".txt";
  ui_file_picker_open("picker","Save",&request);
  assert(click(&ca,"Salva",out,sizeof(out)));
  char expected[512]; snprintf(expected,sizeof(expected),"%s/new.txt",root);
  assert(!strcmp(out,expected));
  assert(access(expected,F_OK)!=0); /* Returning a path never writes its content. */
  request.initial_name="inside";
  ui_file_picker_open("picker","Overwrite",&request);
  assert(!click(&ca,"Salva",out,sizeof(out)) && ui_file_picker_is_open("picker"));
  assert(click(&ca,"Sovrascrivi",out,sizeof(out)) && !strcmp(out,file));
  request.initial_name="../escape";
  ui_file_picker_open("picker","Reject escape",&request);
  assert(!click(&ca,"Salva",out,sizeof(out)) && ui_file_picker_is_open("picker"));
  assert(!ui_file_picker_selected_path("picker",out,sizeof(out)));
  ui_file_picker_cancel(); assert(!ui_has_input_capture());
  request.mode=UI_FILE_PICKER_OPEN; request.initial_name=NULL;
  UIRenderer no_previews=backend;
  no_previews.texture_manager_get_size=NULL;
  no_previews.texture_manager_preview_load=NULL;
  no_previews.texture_manager_preview_clear=NULL;
  assert(ui_context_set_renderer(a,&no_previews,&ca));
  request.image_preview=true;
  ui_file_picker_open("picker","No preview backend",&request);
  assert(!click(&ca,"   inside.txt",out,sizeof(out)));
  snprintf(ca.find,sizeof(ca.find),"Immagine non caricabile"); ca.found=false;
  assert(!frame(&ca,false,-1,-1,out,sizeof(out)) && ca.found);
  assert(click(&ca,"Apri",out,sizeof(out)) && !strcmp(out,file));
  request.image_preview=false;
  multiple_extensions(root,&ca,out,sizeof(out));
  picker_return_focus(root,&ca,out,sizeof(out));
  ui_file_picker_open("picker","First context",&request);
  assert(ui_context_set_current(b) && !ui_file_picker_is_open("picker"));
  ui_file_picker_open("picker","Second context",&request);
  assert(ui_context_reset(a) && ui_file_picker_is_open("picker"));
  assert(ui_context_set_current(a) && !ui_file_picker_is_open("picker"));
  request.root_directory="/not/a/real/picker/root";
  ui_file_picker_open("invalid","Invalid",&request);
  assert(!ui_file_picker_is_open("invalid") && !ui_has_input_capture());
  assert(ui_context_destroy(a));
  assert(ui_context_set_current(b) && ui_file_picker_is_open("picker"));
  assert(ui_context_destroy(b));
  assert(unlink(file)==0 && rmdir(root)==0);
  puts("Picker module: PASS (public API, allocation failure, directory/save/overwrite, containment, cancel, context reset/destroy)");
  return 0;
}
