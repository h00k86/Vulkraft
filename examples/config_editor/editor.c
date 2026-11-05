#include "editor.h"
#include "platform/platform_fs.h"
#include <stdio.h>
#include <string.h>
static void history_reset(ConfigEditor *e) {
  e->history_count=1; e->history_cursor=0; e->history_group=0;
  e->history[0]=(ConfigSnapshot){e->document,e->selected};
}
static bool history_pending(const ConfigEditor *e) {
  return !config_document_equal(&e->document,&e->history[e->history_cursor].document);
}
static bool history_available(const ConfigEditor *e) {
  return !e->confirm && !e->conflict && e->picker==CONFIG_PICKER_NONE && !e->close_ready;
}
void config_editor_history_break(ConfigEditor *e) { e->history_group=0; }
void config_editor_checkpoint(ConfigEditor *e,uint64_t group) {
  if(!history_pending(e)) return;
  if(!group || group!=e->history_group || !e->history_cursor ||
     e->history_cursor+1!=e->history_count) {
    e->history_count=e->history_cursor+1; /* A new edit abandons redo. */
    if(e->history_count==CONFIG_HISTORY_CAP) {
      memmove(e->history,e->history+1,(CONFIG_HISTORY_CAP-1)*sizeof(*e->history));
      --e->history_count; --e->history_cursor;
    }
    ++e->history_cursor; ++e->history_count;
  }
  e->history[e->history_cursor]=(ConfigSnapshot){e->document,e->selected};
  e->history_group=group;
}
bool config_editor_can_undo(const ConfigEditor *e) {
  return history_available(e) && (e->history_cursor>0 || history_pending(e));
}
bool config_editor_can_redo(const ConfigEditor *e) {
  return history_available(e) && !history_pending(e) && e->history_cursor+1<e->history_count;
}
static void history_restore(ConfigEditor *e) {
  e->document=e->history[e->history_cursor].document;
  e->selected=e->history[e->history_cursor].selected;
  e->history_group=0; ++e->generation; /* Discard stale field-local history. */
  e->message[0]=0;
}
bool config_editor_undo(ConfigEditor *e) {
  if(!config_editor_can_undo(e)) return false;
  config_editor_checkpoint(e,0);
  --e->history_cursor; history_restore(e); return true;
}
bool config_editor_redo(ConfigEditor *e) {
  if(!config_editor_can_redo(e)) return false;
  ++e->history_cursor; history_restore(e); return true;
}
bool config_editor_init(ConfigEditor *e,const char *root) {
  *e=(ConfigEditor){0};
  if(!platform_real_path(root,e->root,sizeof(e->root)) || !platform_dir_exists(e->root)) {
    snprintf(e->message,sizeof(e->message),"Directory di lavoro non valida."); return false;
  }
  config_document_init(&e->document); e->saved=e->document; e->generation=1; history_reset(e); return true;
}
bool config_editor_dirty(const ConfigEditor *e) { return !config_document_equal(&e->document,&e->saved); }
static void perform(ConfigEditor *e) {
  ConfigAction action=e->pending; e->pending=CONFIG_ACTION_NONE;
  if(action==CONFIG_ACTION_NEW) {
    config_document_init(&e->document); e->saved=e->document; e->selected=0;
    e->path[0]=0; ++e->generation; e->message[0]=0;
    history_reset(e);
  } else if(action==CONFIG_ACTION_OPEN) e->picker=CONFIG_PICKER_OPEN;
  else if(action==CONFIG_ACTION_CLOSE) e->close_ready=true;
}
void config_editor_request(ConfigEditor *e,ConfigAction action) {
  if(e->confirm || e->conflict || e->picker!=CONFIG_PICKER_NONE || e->close_ready) return;
  config_editor_checkpoint(e,0); config_editor_history_break(e);
  e->pending=action;
  if(config_editor_dirty(e)) e->confirm=true;
  else perform(e);
}
void config_editor_cancel_picker(ConfigEditor *e) { e->picker=CONFIG_PICKER_NONE; e->pending=CONFIG_ACTION_NONE; }
bool config_editor_choose(ConfigEditor *e,const char *path) {
  ConfigPicker picker=e->picker; char resolved[CONFIG_PATH_CAP];
  if(picker==CONFIG_PICKER_NONE) return false;
  if(!platform_resolve_within(e->root,path,picker==CONFIG_PICKER_SAVE,resolved,sizeof(resolved))) {
    snprintf(e->message,sizeof(e->message),"Percorso non accessibile o fuori dalla directory di lavoro.");
    config_editor_cancel_picker(e); return false;
  }
  bool ok;
  if(picker==CONFIG_PICKER_SAVE && e->path[0] &&
     (!strcmp(path,e->path) || !strcmp(resolved,e->path))) {
    ConfigDocument disk; char error[CONFIG_ERROR_CAP];
    /* Compare document content, not timestamp granularity. Missing, unreadable
     * or malformed files cannot be verified and must not be overwritten either.
     * This is an optimistic check, not locking against concurrent writers. */
    if(strcmp(resolved,e->path) || !config_document_load(&disk,resolved,error) ||
       !config_document_equal(&disk,&e->saved)) {
      e->picker=CONFIG_PICKER_NONE; e->conflict=true;
      snprintf(e->message,sizeof(e->message),"Il file e' cambiato o non e' verificabile. Salva una copia per conservare le modifiche.");
      return false;
    }
  }
  if(picker==CONFIG_PICKER_OPEN) {
    ok=config_document_load(&e->document,resolved,e->message);
    if(ok) { e->selected=0; ++e->generation; history_reset(e); }
  } else ok=config_document_save(&e->document,resolved,e->message);
  e->picker=CONFIG_PICKER_NONE;
  if(!ok) { e->pending=CONFIG_ACTION_NONE; return false; }
  e->saved=e->document; snprintf(e->path,sizeof(e->path),"%s",resolved);
  snprintf(e->message,sizeof(e->message),"%s",picker==CONFIG_PICKER_OPEN?"Documento aperto.":"Documento salvato.");
  perform(e); return true;
}
void config_editor_save(ConfigEditor *e,bool save_as) {
  if(e->picker!=CONFIG_PICKER_NONE || e->confirm || e->conflict || e->close_ready) return;
  config_editor_checkpoint(e,0); config_editor_history_break(e);
  e->picker=CONFIG_PICKER_SAVE;
  if(!save_as && e->path[0]) {
    char path[CONFIG_PATH_CAP]; snprintf(path,sizeof(path),"%s",e->path);
    config_editor_choose(e,path);
  }
}
void config_editor_decide(ConfigEditor *e,ConfigDecision decision) {
  if(!e->confirm) return;
  e->confirm=false;
  if(decision==CONFIG_CANCEL) e->pending=CONFIG_ACTION_NONE;
  else if(decision==CONFIG_DISCARD) perform(e);
  else config_editor_save(e,false);
}
void config_editor_resolve_conflict(ConfigEditor *e,bool save_copy) {
  if(!e->conflict) return;
  e->conflict=false;
  if(save_copy) e->picker=CONFIG_PICKER_SAVE;
  else e->pending=CONFIG_ACTION_NONE;
}
