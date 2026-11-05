#define _POSIX_C_SOURCE 200809L
#define main navigation_suite_main
#include "ui_navigation_test.c"
#undef main
#include "../examples/config_editor/view.h"
#include "ui/file_picker.h"
#include <unistd.h>
static ConfigEditor editor;
static void draw(const InputKeyEvent *event) {
  UIFrameInput input={.width=1000,.height=800,.mouse_x=-1,.mouse_y=-1,.events=event,.event_count=event?1:0};
  CHECK(ui_begin_frame_input(&input)); config_editor_draw(&editor,font,1000,800); ui_draw_dropdown_popups(); ui_end_frame();
}
static void send(int k,int mods) { InputKeyEvent event=key(k,mods); draw(&event); }
static bool ends(const char *text,const char *suffix) { size_t n=strlen(text),m=strlen(suffix); return n>=m&&!strcmp(text+n-m,suffix); }
static void target(const char *suffix) {
  for(int i=0;i<100;++i) { if(ends(ui_keyboard_focus_id(),suffix)) return; send(PLATFORM_KEY_TAB,0); }
  fprintf(stderr,"Could not focus %s; current=%s\n",suffix,ui_keyboard_focus_id()); CHECK(false);
}
static void activate(const char *suffix) { target(suffix); send(PLATFORM_KEY_ENTER,0); }
int main(void) {
  char root[]="/tmp/config-editor-ui-XXXXXX"; CHECK(mkdtemp(root)); CHECK(config_editor_init(&editor,root));
  UIContext *c=context(); draw(NULL);
  target("config/1/1/name"); send(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL);
  InputKeyEvent e=character(0x20ac); draw(&e); CHECK(!strcmp(editor.document.items[0].name,"€"));
  activate("config/actions/2"); CHECK(editor.picker==CONFIG_PICKER_SAVE);
  activate("actions/confirm"); CHECK(editor.picker==CONFIG_PICKER_NONE&&!config_editor_dirty(&editor));
  char path[CONFIG_PATH_CAP]; snprintf(path,sizeof(path),"%s",editor.path); CHECK(path[0]);
  activate("config/actions/0"); CHECK(!editor.path[0]&&!strcmp(editor.document.items[0].name,"Profilo 1"));
  activate("config/actions/1"); CHECK(editor.picker==CONFIG_PICKER_OPEN);
  activate("configurazione.cfgapp/entry"); activate("actions/confirm");
  CHECK(!strcmp(editor.document.items[0].name,"€")&&!config_editor_dirty(&editor));
  target("/1/name"); e=character('x'); draw(&e); CHECK(config_editor_dirty(&editor));
  activate("config/actions/0"); CHECK(editor.confirm);
  activate("config/cancel"); CHECK(!editor.confirm&&!strcmp(editor.document.items[0].name,"€x"));
  activate("config/actions/3"); CHECK(editor.picker==CONFIG_PICKER_SAVE);
  activate("actions/confirm"); activate("overwrite_cancel");
  CHECK(editor.picker==CONFIG_PICKER_SAVE&&config_editor_dirty(&editor));
  activate("actions/cancel"); CHECK(editor.picker==CONFIG_PICKER_NONE&&config_editor_dirty(&editor));
  ConfigDocument external=editor.saved; char conflict_error[CONFIG_ERROR_CAP];
  strcpy(external.items[0].note,"Esterno"); CHECK(config_document_save(&external,path,conflict_error));
  activate("config/actions/2"); CHECK(editor.conflict&&config_editor_dirty(&editor));
  send(PLATFORM_KEY_ESCAPE,0); CHECK(!editor.conflict&&config_editor_dirty(&editor));
  activate("config/actions/2"); CHECK(editor.conflict);
  activate("config/conflict-copy"); CHECK(editor.picker==CONFIG_PICKER_SAVE);
  target("filename"); send(PLATFORM_KEY_A,PLATFORM_MOD_CONTROL);
  for(const char *s="copia.cfgapp";*s;++s) { e=character((unsigned char)*s); draw(&e); }
  activate("actions/confirm"); CHECK(!editor.conflict&&!config_editor_dirty(&editor)&&strcmp(path,editor.path));
  ConfigDocument untouched; CHECK(config_document_load(&untouched,path,conflict_error)&&!strcmp(untouched.items[0].note,"Esterno"));
  CHECK(unlink(path)==0); snprintf(path,sizeof(path),"%s",editor.path);
  /* Document history operates through the public view; text undo remains local. */
  activate("config/add"); CHECK(editor.document.count==2&&editor.selected==1);
  activate("config/history/0"); CHECK(editor.document.count==1&&!config_editor_dirty(&editor));
  activate("config/history/1"); CHECK(editor.document.count==2&&editor.selected==1);
  activate("config/remove"); CHECK(editor.document.count==1);
  activate("config/history/0"); CHECK(editor.document.count==2);
  activate("config/history/0"); CHECK(editor.document.count==1&&!config_editor_dirty(&editor));
  target("/1/name"); e=character('a'); draw(&e); e=character('b'); draw(&e);
  CHECK(!strcmp(editor.document.items[0].name,"€xab"));
  activate("config/history/0"); CHECK(!strcmp(editor.document.items[0].name,"€x")&&!config_editor_dirty(&editor));
  activate("config/history/1"); CHECK(!strcmp(editor.document.items[0].name,"€xab"));
  activate("config/history/0"); CHECK(!config_editor_dirty(&editor));
  target("/1/name"); e=character('y'); draw(&e); CHECK(config_editor_dirty(&editor));
  activate("config/actions/4"); CHECK(editor.confirm&&!editor.close_ready);
  activate("config/save-continue"); CHECK(editor.close_ready&&!config_editor_dirty(&editor));
  ConfigDocument loaded; char error[CONFIG_ERROR_CAP]; CHECK(config_document_load(&loaded,path,error)&&!strcmp(loaded.items[0].name,"€xy"));
  CHECK(ui_context_destroy(c)); CHECK(unlink(path)==0&&rmdir(root)==0);
  puts("Config editor UI: PASS (edit, Save picker, New, Open picker, cancel dirty/overwrite, conflict copy/cancel, save before close)");
  return 0;
}
