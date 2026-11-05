#define _POSIX_C_SOURCE 200809L
#define main navigation_fixture_main
#include "ui_navigation_test.c"
#undef main
#include "../examples/config_editor/view.h"
#include "ui/draw_list.h"
#include "ui/file_picker.h"
#include "platform/platform_fs.h"
#include "../tools/ui_benchmark_metrics.h"
#include <unistd.h>

static ConfigEditor editor,reopened;
static UIDrawList *list;
static unsigned seed_index,mode,cycle;
static size_t frames,sessions,failed_saves,conflicts,reopens;
#undef CHECK
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"editor session seed=%u mode=%u cycle=%u frame=%zu line=%d: %s\n",seed_index,mode,cycle,frames,__LINE__,#x); exit(1); } } while(0)
static bool fail_write;
static size_t injected_writes;
size_t __real_fwrite(const void *,size_t,size_t,FILE *);
size_t __wrap_fwrite(const void *data,size_t size,size_t count,FILE *file) {
  if(fail_write&&count) { ++injected_writes; return __real_fwrite(data,size,count/2,file); }
  return __real_fwrite(data,size,count,file);
}
static unsigned next_random(unsigned *state) { *state=*state*1664525u+1013904223u; return *state; }
static void draw_event(const InputKeyEvent *event) {
  if(list) CHECK(ui_draw_list_begin(list));
  UIFrameInput input={.width=1000,.height=800,.mouse_x=-1,.mouse_y=-1,
    .delta_time=.016f,.events=event,.event_count=event?1:0};
  CHECK(ui_begin_frame_input_status(&input)==UI_ERROR_NONE);
  config_editor_draw(&editor,font,1000,800); ui_draw_dropdown_popups(); ui_end_frame();
  if(list) CHECK(ui_draw_list_replay(list));
  CHECK(!clips&&ui_context_error(ui_context_get_current()).code==UI_ERROR_NONE);
  CHECK(editor.history_count>=1&&editor.history_count<=CONFIG_HISTORY_CAP);
  CHECK(editor.history_cursor>=0&&editor.history_cursor<editor.history_count);
  CHECK(editor.document.count>=1&&editor.document.count<=CONFIG_MAX_ITEMS);
  CHECK(editor.selected>=0&&editor.selected<editor.document.count);
  if(event) CHECK(!event->consumed);
  ++frames;
}
static void send_key(int code,int mods) { InputKeyEvent e=key(code,mods); draw_event(&e); }
static void send_character(unsigned cp) { InputKeyEvent e=character(cp); draw_event(&e); }
static bool suffix_is(const char *text,const char *suffix) {
  size_t n=strlen(text),m=strlen(suffix); return n>=m&&!strcmp(text+n-m,suffix);
}
static void target(const char *suffix) {
  for(int i=0;i<160;++i) {
    if(suffix_is(ui_keyboard_focus_id(),suffix)) return;
    send_key(PLATFORM_KEY_TAB,0);
  }
  fprintf(stderr,"Missing target %s; current %s\n",suffix,ui_keyboard_focus_id()); CHECK(false);
}
static void activate(const char *suffix) { target(suffix); send_key(PLATFORM_KEY_ENTER,0); }
static void replace_ascii(const char *suffix,const char *value) {
  target(suffix); send_key(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL);
  for(const unsigned char *p=(const unsigned char *)value;*p;++p) send_character(*p);
}
static void on_disk(const char *path,const ConfigDocument *expected) {
  ConfigDocument loaded; char error[CONFIG_ERROR_CAP];
  CHECK(config_document_load(&loaded,path,error)&&config_document_equal(&loaded,expected));
}
static void field_suffix(char *out,size_t size,const char *field_name) {
  snprintf(out,size,"/%u/%s",editor.document.items[editor.selected].id,field_name);
}
static void workflow(const char *root,unsigned random) {
  activate("config/actions/0"); CHECK(!editor.path[0]&&!config_editor_dirty(&editor));
  /* Fixture construction uses the document API. All subsequent edits,
   * decisions, history, picker and save actions traverse the real view. */
  const int counts[]={1,4,32}; int count=counts[cycle%3];
  while(editor.document.count<count) CHECK(config_document_add(&editor.document));
  for(int i=0;i<count;++i) {
    snprintf(editor.document.items[i].name,CONFIG_NAME_CAP,"Profilo è %u/%u/%d",seed_index,cycle,i);
    editor.document.items[i].level=(int)((random+(unsigned)i)%101);
  }
  editor.selected=(int)(random%(unsigned)count); config_editor_checkpoint(&editor,0); draw_event(NULL);
  ConfigDocument initial=editor.document;
  activate("config/actions/2"); CHECK(editor.picker==CONFIG_PICKER_SAVE);
  activate("actions/confirm"); CHECK(!config_editor_dirty(&editor)&&editor.path[0]);
  char path[CONFIG_PATH_CAP],copy[CONFIG_PATH_CAP],suffix[64],error[CONFIG_ERROR_CAP];
  snprintf(path,sizeof(path),"%s",editor.path); snprintf(copy,sizeof(copy),"%s/copy.cfgapp",root);
  on_disk(path,&initial);
  field_suffix(suffix,sizeof(suffix),"name"); target(suffix); send_key(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL);
  send_character(0xe8); send_character(0x20ac); send_character('A'+random%26);
  char wanted[32]; snprintf(wanted,sizeof(wanted),"è€%c",'A'+(int)(random%26));
  CHECK(!strcmp(editor.document.items[editor.selected].name,wanted));
  send_key(PLATFORM_KEY_Z,PLATFORM_MOD_CONTROL); CHECK(!strcmp(editor.document.items[editor.selected].name,"è€"));
  send_key(PLATFORM_KEY_Y,PLATFORM_MOD_CONTROL); CHECK(!strcmp(editor.document.items[editor.selected].name,wanted));
  send_key(PLATFORM_KEY_ENTER,0); ConfigDocument edited=editor.document;
  activate("config/history/0"); CHECK(config_document_equal(&editor.document,&initial)&&!config_editor_dirty(&editor));
  activate("config/history/1"); CHECK(config_document_equal(&editor.document,&edited)&&config_editor_dirty(&editor));

  target("config/actions/2"); int history=editor.history_count;
  size_t before=injected_writes; fail_write=true; send_key(PLATFORM_KEY_ENTER,0); fail_write=false;
  CHECK(injected_writes>before&&config_editor_dirty(&editor)&&editor.history_count==history);
  CHECK(config_document_equal(&editor.document,&edited)&&editor.message[0]); on_disk(path,&initial); ++failed_saves;
  activate(cycle%2?"config/actions/4":"config/actions/0"); CHECK(editor.confirm&&!editor.close_ready);
  CHECK(!config_editor_undo(&editor)); activate("config/cancel");
  CHECK(!editor.confirm&&!editor.close_ready&&config_document_equal(&editor.document,&edited));
  activate("config/actions/2"); CHECK(!config_editor_dirty(&editor)); on_disk(path,&edited);
  activate("config/history/0"); CHECK(config_editor_dirty(&editor)&&config_document_equal(&editor.document,&initial));
  activate("config/actions/2"); CHECK(config_editor_can_redo(&editor)); on_disk(path,&initial);
  activate("config/history/1"); CHECK(config_editor_dirty(&editor)&&config_document_equal(&editor.document,&edited));
  activate("config/actions/2"); CHECK(!config_editor_dirty(&editor)); on_disk(path,&edited);

  ConfigDocument external=editor.saved; snprintf(external.items[0].note,CONFIG_NOTE_CAP,"Esterno %u/%u",seed_index,cycle);
  CHECK(config_document_save(&external,path,error));
  field_suffix(suffix,sizeof(suffix),"note"); replace_ascii(suffix,"Locale "); send_character(0x20ac); send_key(PLATFORM_KEY_ENTER,0);
  ConfigDocument local=editor.document;
  activate("config/actions/2"); CHECK(editor.conflict&&config_editor_dirty(&editor)); on_disk(path,&external); ++conflicts;
  send_key(PLATFORM_KEY_ESCAPE,0); CHECK(!editor.conflict&&config_document_equal(&editor.document,&local));
  activate("config/actions/2"); CHECK(editor.conflict); activate("config/conflict-copy");
  CHECK(editor.picker==CONFIG_PICKER_SAVE); replace_ascii("filename","copy.cfgapp");
  activate("actions/confirm"); CHECK(!editor.conflict&&!config_editor_dirty(&editor)&&!strcmp(editor.path,copy));
  on_disk(path,&external); on_disk(copy,&local);

  activate("config/actions/0"); CHECK(editor.document.count==1&&!editor.path[0]);
  activate("config/actions/1"); CHECK(editor.picker==CONFIG_PICKER_OPEN);
  activate("copy.cfgapp/entry"); activate("actions/confirm");
  CHECK(config_document_equal(&editor.document,&local)&&!config_editor_dirty(&editor)); ++reopens;
  field_suffix(suffix,sizeof(suffix),"note"); target(suffix); send_key(PLATFORM_KEY_END,0); send_character('!'); send_key(PLATFORM_KEY_ENTER,0);
  CHECK(config_editor_dirty(&editor)); ConfigDocument final=editor.document;
  activate("config/actions/4"); CHECK(editor.confirm&&!editor.close_ready);
  activate("config/cancel"); CHECK(!editor.close_ready&&config_editor_dirty(&editor));
  if(cycle%4==3) {
    activate("config/actions/4"); activate("config/save-continue");
    CHECK(editor.close_ready&&!config_editor_dirty(&editor)); on_disk(copy,&final);
    /* Fresh controller: a reopened process must recover the saved document,
     * with no inherited field/document history. A fresh UI is created below. */
    CHECK(config_editor_init(&reopened,root)); config_editor_request(&reopened,CONFIG_ACTION_OPEN);
    CHECK(config_editor_choose(&reopened,copy)&&config_document_equal(&reopened.document,&final));
    CHECK(!config_editor_can_undo(&reopened)&&!config_editor_dirty(&reopened)); ++reopens;
  } else { activate("config/actions/2"); CHECK(!config_editor_dirty(&editor)); on_disk(copy,&final); }
  CHECK(unlink(path)==0&&unlink(copy)==0);
  PlatformDirIter it; PlatformDirEntry entry; CHECK(platform_dir_open(&it,root));
  while(platform_dir_next(&it,&entry)) CHECK(!strstr(entry.name,".tmp."));
  platform_dir_close(&it); ++sessions;
}
int main(void) {
  const unsigned seeds[]={1,0x12345678u,0xc0ffeeu};
  char root[]="/tmp/config-editor-session-XXXXXX"; CHECK(mkdtemp(root));
  for(seed_index=0;seed_index<3;++seed_index) for(mode=0;mode<2;++mode) {
    unsigned random=seeds[seed_index]; CHECK(config_editor_init(&editor,root));
    for(unsigned block=0;block<3;++block) {
      UIContext *ctx=context();
      if(mode) { list=ui_draw_list_create(&renderer,NULL); CHECK(list&&ui_context_set_renderer(ctx,ui_draw_list_renderer(),list)); }
      draw_event(NULL);
      for(cycle=block*4;cycle<(block+1)*4;++cycle) workflow(root,next_random(&random));
      CHECK(ui_context_set_renderer(ctx,&renderer,NULL)); ui_draw_list_destroy(list); list=NULL;
      CHECK(ui_context_destroy(ctx)&&!bench_metrics.live&&!bench_metrics.unknown_frees);
      editor=reopened;
    }
  }
  CHECK(rmdir(root)==0&&!clips&&!ui_context_get_current());
  printf("Editor sessions: PASS (3 seeds, 2 render modes, %zu workflows, %zu frames, %zu failed writes, %zu external conflicts, %zu reopens, peak=%zu, final observed live=0)\n",sessions,frames,failed_saves,conflicts,reopens,bench_metrics.peak);
  bench_reset_metrics();
}
