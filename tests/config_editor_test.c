#define _POSIX_C_SOURCE 200809L
#include "../examples/config_editor/editor.h"
#include "platform/platform_fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"config:%d: %s\n",__LINE__,#x); exit(1); } } while(0)
#ifdef CONFIG_TEST_FAULT_IO
static bool fail_write;
size_t __real_fwrite(const void *,size_t,size_t,FILE *);
size_t __wrap_fwrite(const void *data,size_t size,size_t count,FILE *file) {
  if(fail_write && count) return __real_fwrite(data,size,count/2,file);
  return __real_fwrite(data,size,count,file);
}
#endif
static void raw(const char *path,const char *data) { CHECK(platform_write_file_atomic(path,data,strlen(data))); }
static void history_test(const char *root) {
  static ConfigEditor e;
  CHECK(config_editor_init(&e,root));
  CHECK(!config_editor_can_undo(&e)&&!config_editor_can_redo(&e));
  ConfigDocument initial=e.document;
  strcpy(e.document.items[0].name,"Caffè"); config_editor_checkpoint(&e,5);
  strcpy(e.document.items[0].name,"Caffè €"); config_editor_checkpoint(&e,5);
  CHECK(e.history_count==2&&config_editor_undo(&e));
  CHECK(config_document_equal(&e.document,&initial)&&!config_editor_dirty(&e));
  CHECK(config_editor_redo(&e)&&!strcmp(e.document.items[0].name,"Caffè €"));
  config_editor_history_break(&e);
  CHECK(config_document_add(&e.document)); e.selected=1; config_editor_checkpoint(&e,0);
  unsigned next=e.document.next_id;
  CHECK(config_document_remove(&e.document,1)); e.selected=0; config_editor_checkpoint(&e,0);
  CHECK(config_editor_undo(&e)&&e.document.count==2&&e.selected==1);
  CHECK(config_editor_undo(&e)&&e.document.count==1&&e.document.next_id==next-1);
  CHECK(config_editor_redo(&e)&&e.document.count==2&&e.document.next_id==next);
  e.document.items[1].enabled=false; config_editor_checkpoint(&e,0);
  CHECK(!config_editor_can_redo(&e)); /* New edit cuts the old redo branch. */
  char path[CONFIG_PATH_CAP]; snprintf(path,sizeof(path),"%s/history.cfgapp",root);
  config_editor_save(&e,true); CHECK(config_editor_choose(&e,path));
  CHECK(!config_editor_dirty(&e));
  CHECK(config_editor_undo(&e)&&config_editor_dirty(&e));
  CHECK(config_editor_redo(&e)&&!config_editor_dirty(&e));
  /* Temporary invalid text is undoable; failed saving preserves history. */
  e.document.items[1].name[0]=0; config_editor_checkpoint(&e,9);
  int count=e.history_count;
  config_editor_save(&e,false); CHECK(config_editor_dirty(&e)&&e.history_count==count);
  CHECK(config_editor_undo(&e)&&!config_editor_dirty(&e));
  e.document.items[0].level=70; config_editor_checkpoint(&e,0);
  config_editor_request(&e,CONFIG_ACTION_CLOSE);
  CHECK(e.confirm&&!config_editor_undo(&e)&&!config_editor_redo(&e));
  config_editor_decide(&e,CONFIG_CANCEL); CHECK(config_editor_undo(&e));
  config_editor_request(&e,CONFIG_ACTION_OPEN);
  CHECK(!config_editor_undo(&e)); config_editor_cancel_picker(&e);
  CHECK(config_editor_can_redo(&e));
  config_editor_request(&e,CONFIG_ACTION_OPEN); CHECK(config_editor_choose(&e,path));
  CHECK(!config_editor_can_undo(&e)&&!config_editor_can_redo(&e));
  config_editor_request(&e,CONFIG_ACTION_NEW); CHECK(e.history_count==1&&!e.path[0]);
  for(int i=1;i<=40;++i) { e.document.items[0].level=i; config_editor_checkpoint(&e,0); }
  CHECK(e.history_count==CONFIG_HISTORY_CAP);
  int steps=0; while(config_editor_undo(&e)) ++steps;
  CHECK(steps==32&&e.document.items[0].level==8);
  steps=0; while(config_editor_redo(&e)) ++steps;
  CHECK(steps==32&&e.document.items[0].level==40);
  CHECK(unlink(path)==0);
}
int main(void) {
  char root[]="/tmp/config-editor-test-XXXXXX"; CHECK(mkdtemp(root));
  char path[CONFIG_PATH_CAP],bad[CONFIG_PATH_CAP],missing[CONFIG_PATH_CAP],error[CONFIG_ERROR_CAP];
  snprintf(path,sizeof(path),"%s/test.cfgapp",root); snprintf(bad,sizeof(bad),"%s/bad.cfgapp",root);
  snprintf(missing,sizeof(missing),"%s/missing/test.cfgapp",root);
  ConfigDocument d,loaded; config_document_init(&d);
  strcpy(d.items[0].name,"Caffè € 🙂"); strcpy(d.items[0].note,"Profilo di prova");
  CHECK(config_document_add(&d)); d.items[1].level=73; d.items[1].enabled=false;
  CHECK(config_document_save(&d,path,error)); config_document_init(&loaded);
  CHECK(config_document_load(&loaded,path,error)&&config_document_equal(&d,&loaded));
  const char *invalid[]={"", "CONFIG_EDITOR\t2\t2\n", "CONFIG_EDITOR\t1\t2\n1\t1\t50\t\t\n",
    "CONFIG_EDITOR\t1\t2\n1\t1\t101\tx\t\n", "CONFIG_EDITOR\t1\t2\n1\t1\t50\tx\t\n1\t1\t50\ty\t\n",
    "CONFIG_EDITOR\t1\t2\n1\t1\t50\t\xc0\xaf\t\n", "CONFIG_EDITOR\t1\t2\n1\t1\t50\tx\t",
    "CONFIG_EDITOR\t1\t999999999999999\n", "CONFIG_EDITOR\t1\t2\n2\t1\t50\tx\t\n"};
  for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i) { raw(bad,invalid[i]); CHECK(!config_document_load(&loaded,bad,error)&&config_document_equal(&d,&loaded)); }
  char *large=malloc(CONFIG_FILE_CAP+2); CHECK(large); memset(large,'a',CONFIG_FILE_CAP+1);
  CHECK(platform_write_file_atomic(bad,large,CONFIG_FILE_CAP+1)); free(large);
  CHECK(!config_document_load(&loaded,bad,error)&&config_document_equal(&d,&loaded));
  d.items[0].name[0]=0; CHECK(!config_document_save(&d,path,error));
  CHECK(config_document_load(&d,path,error)&&config_document_equal(&d,&loaded));
#ifdef CONFIG_TEST_FAULT_IO
  fail_write=true; d.items[0].level=91;
  CHECK(!config_document_save(&d,path,error)); fail_write=false;
  ConfigDocument preserved; CHECK(config_document_load(&preserved,path,error)&&config_document_equal(&preserved,&loaded));
  d=loaded;
#endif
  CHECK(!config_document_save(&d,missing,error)); CHECK(!config_document_save(&d,root,error));
  char link[CONFIG_PATH_CAP]; snprintf(link,sizeof(link),"%s/link.cfgapp",root);
  CHECK(symlink(path,link)==0); CHECK(!config_document_save(&d,link,error)); CHECK(unlink(link)==0);
  if(geteuid()!=0) { CHECK(chmod(root,0500)==0); CHECK(!config_document_save(&d,path,error)); CHECK(chmod(root,0700)==0); }
  CHECK(config_document_load(&d,path,error)&&config_document_equal(&d,&loaded));
  ConfigEditor e; CHECK(config_editor_init(&e,root)); CHECK(!config_editor_dirty(&e));
  strcpy(e.document.items[0].name,"Modificato"); CHECK(config_editor_dirty(&e));
  config_editor_request(&e,CONFIG_ACTION_CLOSE); CHECK(e.confirm&&!e.close_ready);
  config_editor_decide(&e,CONFIG_CANCEL); CHECK(!e.confirm&&!e.close_ready&&config_editor_dirty(&e));
  config_editor_request(&e,CONFIG_ACTION_NEW); config_editor_decide(&e,CONFIG_SAVE); CHECK(e.picker==CONFIG_PICKER_SAVE);
  config_editor_cancel_picker(&e); CHECK(!strcmp(e.document.items[0].name,"Modificato")&&config_editor_dirty(&e));
  config_editor_request(&e,CONFIG_ACTION_OPEN); config_editor_decide(&e,CONFIG_DISCARD);
  CHECK(!config_editor_choose(&e,bad)&&!strcmp(e.document.items[0].name,"Modificato")&&config_editor_dirty(&e));
  config_editor_request(&e,CONFIG_ACTION_OPEN); config_editor_decide(&e,CONFIG_DISCARD);
  config_editor_cancel_picker(&e); CHECK(config_editor_dirty(&e));
  config_editor_request(&e,CONFIG_ACTION_CLOSE); config_editor_decide(&e,CONFIG_SAVE);
  CHECK(!config_editor_choose(&e,missing)&&!e.close_ready&&config_editor_dirty(&e));
  config_editor_save(&e,true); CHECK(config_editor_choose(&e,path)); CHECK(!config_editor_dirty(&e)&&!e.close_ready);
  strcpy(e.document.items[0].note,"Nuova nota"); config_editor_request(&e,CONFIG_ACTION_NEW); config_editor_decide(&e,CONFIG_SAVE);
  CHECK(!e.path[0]&&!config_editor_dirty(&e));
  CHECK(config_document_load(&loaded,path,error)&&!strcmp(loaded.items[0].note,"Nuova nota"));
  config_editor_request(&e,CONFIG_ACTION_OPEN); CHECK(config_editor_choose(&e,path)); CHECK(config_document_equal(&e.document,&loaded));
  strcpy(e.document.items[0].note,"Scarta"); config_editor_request(&e,CONFIG_ACTION_CLOSE); config_editor_decide(&e,CONFIG_DISCARD); CHECK(e.close_ready);
  CHECK(config_editor_init(&e,root)); config_editor_request(&e,CONFIG_ACTION_OPEN); CHECK(!config_editor_choose(&e,"/etc/passwd"));
  CHECK(config_document_remove(&e.document,0)); CHECK(e.document.count==0); CHECK(config_document_add(&e.document)); CHECK(e.document.items[0].id==2);
  /* Optimistic conflict detection preserves both local and external changes. */
  CHECK(config_editor_init(&e,root)); config_editor_request(&e,CONFIG_ACTION_OPEN);
  CHECK(config_editor_choose(&e,path)); ConfigDocument external=e.document;
  strcpy(e.document.items[0].note,"Locale"); strcpy(external.items[0].note,"Esterno");
  CHECK(config_document_save(&external,path,error));
  config_editor_request(&e,CONFIG_ACTION_CLOSE); config_editor_decide(&e,CONFIG_SAVE);
  CHECK(e.conflict&&!e.close_ready&&config_editor_dirty(&e));
  CHECK(config_document_load(&loaded,path,error)&&config_document_equal(&loaded,&external));
  config_editor_resolve_conflict(&e,false);
  CHECK(!e.conflict&&e.pending==CONFIG_ACTION_NONE&&!strcmp(e.document.items[0].note,"Locale"));
  config_editor_save(&e,true); CHECK(!config_editor_choose(&e,path)&&e.conflict); /* Save As is no bypass. */
  config_editor_resolve_conflict(&e,true); config_editor_cancel_picker(&e);
  CHECK(config_editor_dirty(&e)&&!e.close_ready);
  config_editor_request(&e,CONFIG_ACTION_CLOSE); config_editor_decide(&e,CONFIG_SAVE);
  CHECK(e.conflict); config_editor_resolve_conflict(&e,true);
  char copy[CONFIG_PATH_CAP]; snprintf(copy,sizeof(copy),"%s/copy.cfgapp",root);
  CHECK(config_editor_choose(&e,copy)&&e.close_ready&&!config_editor_dirty(&e));
  CHECK(config_document_load(&loaded,copy,error)&&!strcmp(loaded.items[0].note,"Locale"));
  CHECK(config_document_load(&loaded,path,error)&&!strcmp(loaded.items[0].note,"Esterno"));
  CHECK(config_editor_init(&e,root)); config_editor_request(&e,CONFIG_ACTION_OPEN); CHECK(config_editor_choose(&e,path));
  CHECK(unlink(path)==0); config_editor_save(&e,false); CHECK(e.conflict&&!platform_path_exists(path));
  config_editor_resolve_conflict(&e,false); raw(path,"malformed"); config_editor_save(&e,false); CHECK(e.conflict);
  config_editor_resolve_conflict(&e,false);
  CHECK(unlink(copy)==0);
  PlatformDirIter it; PlatformDirEntry entry; CHECK(platform_dir_open(&it,root));
  while(platform_dir_next(&it,&entry)) CHECK(!strstr(entry.name,".tmp."));
  platform_dir_close(&it);
  history_test(root);
  CHECK(unlink(path)==0&&unlink(bad)==0&&rmdir(root)==0);
  puts("Config editor: PASS (round-trip UTF-8, strict parser, atomic write failure, dirty/save/discard/cancel, external conflicts, root containment)");
  return 0;
}
