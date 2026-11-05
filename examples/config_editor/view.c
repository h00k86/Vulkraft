#include "view.h"
#include "ui/file_picker.h"
#include <stdio.h>
#include <string.h>
static const char *picker_id="config/picker";
static void picker(ConfigEditor *e,Font *font) {
  if(e->picker==CONFIG_PICKER_NONE) return;
  if(!ui_file_picker_is_open(picker_id)) {
    UIFilePickerRequest request={.mode=e->picker==CONFIG_PICKER_OPEN?UI_FILE_PICKER_OPEN:UI_FILE_PICKER_SAVE,
      .root_directory=e->root,.extension=".cfgapp",.initial_name="configurazione.cfgapp"};
    ui_file_picker_open(picker_id,e->picker==CONFIG_PICKER_OPEN?"Apri configurazione":"Salva configurazione",&request);
    if(!ui_file_picker_is_open(picker_id)) { config_editor_cancel_picker(e); snprintf(e->message,sizeof(e->message),"Impossibile aprire il selettore file."); return; }
  }
  char path[CONFIG_PATH_CAP];
  if(ui_file_picker_draw(font,path,sizeof(path))) config_editor_choose(e,path);
  else if(!ui_file_picker_is_open(picker_id)) config_editor_cancel_picker(e);
}
void config_editor_draw(ConfigEditor *e,Font *font,float width,float height) {
  if(width<240 || height<240) return;
  UIPanel panel=ui_panel_begin("config","Editor di configurazione",font,(UIRect){16,16,width-32,height-32},1);
  if(panel.clip_pushed) {
    UIScrollScope scroll=ui_scroll_begin("config/scroll",panel.content,1);
    if(scroll.clip_pushed) {
      UIPropertyLayout p=ui_property_layout_begin(scroll.layout.bounds.x,scroll.layout.cursor_y,scroll.layout.bounds.w,1);
      ui_property_value(&p,font,"Documento",e->path[0]?e->path:"Senza nome");
      ui_property_value(&p,font,"Stato",config_editor_dirty(e)?"Modifiche non salvate":"Nessuna modifica da salvare");
      const char *actions[]={"Nuovo","Apri","Salva","Salva come","Chiudi"};
      unsigned choice=ui_property_action_row(&p,"config/actions",font,actions,5);
      if(choice&1) config_editor_request(e,CONFIG_ACTION_NEW);
      if(choice&2) config_editor_request(e,CONFIG_ACTION_OPEN);
      if(choice&4) config_editor_save(e,false);
      if(choice&8) config_editor_save(e,true);
      if(choice&16) config_editor_request(e,CONFIG_ACTION_CLOSE);
      ui_property_note(&p,font,e->message[0]?e->message:"Seleziona un profilo e modifica le sue proprieta'.");
      ui_property_layout_gap(&p,8);
      ui_property_value(&p,font,"Profili","Seleziona una voce");
      for(int i=0;i<e->document.count;++i) {
        ConfigItem *v=&e->document.items[i]; char id[96],label[128];
        snprintf(id,sizeof(id),"config/%u/item/%u",e->generation,v->id);
        snprintf(label,sizeof(label),"%s%s",i==e->selected?"> ":"",v->name[0]?v->name:"(senza nome)");
        if(ui_property_action(&p,id,font,label,false)) { config_editor_history_break(e); e->selected=i; }
      }
      if(e->document.count<CONFIG_MAX_ITEMS && ui_property_action(&p,"config/add",font,"Aggiungi profilo",false)) {
        if(config_document_add(&e->document)) e->selected=e->document.count-1;
        config_editor_checkpoint(e,0);
      }
      if(e->selected>=e->document.count) e->selected=e->document.count-1;
      if(e->selected>=0 && e->selected<e->document.count) {
        ConfigItem *v=&e->document.items[e->selected]; char id[96];
        ui_property_layout_gap(&p,12);
        ui_property_value(&p,font,"Proprieta'","Le modifiche al testo sono immediate");
        snprintf(id,sizeof(id),"config/%u/%u/name",e->generation,v->id);
        ui_property_text(&p,id,font,"Nome",v->name,sizeof(v->name));
        uint64_t name_group=((uint64_t)v->id<<2)|1;
        config_editor_checkpoint(e,name_group);
        if(!ui_text_input_is_focused(id) && e->history_group==name_group) config_editor_history_break(e);
        snprintf(id,sizeof(id),"config/%u/%u/note",e->generation,v->id);
        ui_property_text(&p,id,font,"Descrizione",v->note,sizeof(v->note));
        uint64_t note_group=((uint64_t)v->id<<2)|2;
        config_editor_checkpoint(e,note_group);
        if(!ui_text_input_is_focused(id) && e->history_group==note_group) config_editor_history_break(e);
        snprintf(id,sizeof(id),"config/%u/%u/enabled",e->generation,v->id);
        ui_property_checkbox(&p,id,font,"Abilitato",&v->enabled);
        config_editor_checkpoint(e,0);
        char level[32]; snprintf(level,sizeof(level),"%d / 100",v->level);
        ui_property_value(&p,font,"Livello",level);
        const char *levels[]={"-10","+10"};
        unsigned change=ui_property_action_row(&p,"config/level",font,levels,2);
        if(change&1) { v->level-=10; if(v->level<0) v->level=0; }
        if(change&2) { v->level+=10; if(v->level>100) v->level=100; }
        config_editor_checkpoint(e,0);
        if(ui_property_action(&p,"config/remove",font,"Rimuovi profilo",true)) {
          config_document_remove(&e->document,e->selected);
          if(e->selected>=e->document.count) e->selected=e->document.count-1;
          config_editor_checkpoint(e,0);
        }
      }
      ui_property_layout_gap(&p,12);
      ui_property_value(&p,font,"Cronologia documento",
        config_editor_can_undo(e)?"Modifiche annullabili":config_editor_can_redo(e)?"Modifiche ripristinabili":"Nessuna modifica annullabile");
      const char *history_actions[]={"Annulla modifica","Ripristina modifica"};
      unsigned history=ui_property_action_row(&p,"config/history",font,history_actions,2);
      if(history&1) config_editor_undo(e);
      if(history&2) config_editor_redo(e);
      ui_property_layout_commit(&scroll.layout,&p);
    }
    ui_scroll_end(&scroll);
  }
  ui_panel_end(&panel);
  if(e->confirm) {
    bool open=true; float x,y,w,h; ConfigDecision decision=CONFIG_CANCEL; bool decided=false;
    if(ui_modal_begin("config/unsaved","Modifiche non salvate",font,520,230,&open,&x,&y,&w,&h)) {
      UIPropertyLayout p=ui_property_layout_begin(x+12,y+12,w-24,1);
      ui_property_note(&p,font,"Salvare prima di continuare?");
      if(ui_property_action(&p,"config/save-continue",font,"Salva e continua",false)) { decision=CONFIG_SAVE; decided=true; }
      if(ui_property_action(&p,"config/discard",font,"Continua senza salvare",true)) { decision=CONFIG_DISCARD; decided=true; }
      if(ui_property_action(&p,"config/cancel",font,"Annulla",false)) decided=true;
    }
    ui_modal_end("config/unsaved",open&&!decided);
    if(!open || decided) config_editor_decide(e,decision);
  }
  if(e->conflict) {
    bool open=true,copy=false,cancel=false; float x,y,w,h;
    if(ui_modal_begin("config/conflict","File modificato all'esterno",font,580,250,&open,&x,&y,&w,&h)) {
      UIPropertyLayout p=ui_property_layout_begin(x+12,y+12,w-24,1);
      ui_property_note(&p,font,"Il file e' cambiato, e' stato rimosso o non e' leggibile.");
      ui_property_note(&p,font,"Le modifiche locali sono ancora disponibili.");
      copy=ui_property_action(&p,"config/conflict-copy",font,"Salva una copia in un altro file",false);
      cancel=ui_property_action(&p,"config/conflict-cancel",font,"Annulla",false);
    }
    ui_modal_end("config/conflict",open&&!copy&&!cancel);
    if(!open || copy || cancel) config_editor_resolve_conflict(e,copy);
  }
  picker(e,font);
  ui_debug_log_input_events(); /* Opt-in UI_INPUT_DEBUG, no logging by default. */
}
size_t config_editor_smoke_event(int frame,InputKeyEvent *event) {
  /* Five toolbar stops, one item, Add, then Name. Entering text dirties the
   * document. Native close/confirmation is exercised separately by the host. */
  if(frame>=1 && frame<=8) { *event=(InputKeyEvent){.kind=INPUT_KEY,.key=PLATFORM_KEY_TAB,.action=PLATFORM_PRESS}; return 1; }
  if(frame==14) { *event=(InputKeyEvent){.kind=INPUT_KEY,.key=PLATFORM_KEY_ESCAPE,.action=PLATFORM_PRESS}; return 1; }
  if(frame==9) { *event=(InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint=0xe8}; return 1; }
  return 0;
}
bool config_editor_smoke_result(const ConfigEditor *e) {
  return !e->confirm && !e->close_ready && e->pending==CONFIG_ACTION_NONE && config_editor_dirty(e) && !strcmp(e->document.items[0].name,"Profilo 1è");
}
