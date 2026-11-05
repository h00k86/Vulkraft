#include "ui_reference_scene.h"
#include "ui/file_picker.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
size_t ui_reference_navigation_events(int frame,InputKeyEvent *event) {
  static const int keys[]={0,PLATFORM_KEY_TAB,0,PLATFORM_KEY_TAB,PLATFORM_KEY_SPACE,
    PLATFORM_KEY_TAB,PLATFORM_KEY_RIGHT,PLATFORM_KEY_TAB,PLATFORM_KEY_TAB,
    PLATFORM_KEY_ENTER,PLATFORM_KEY_DOWN,PLATFORM_KEY_ENTER,PLATFORM_KEY_TAB,
    PLATFORM_KEY_SPACE,PLATFORM_KEY_TAB,PLATFORM_KEY_ENTER,PLATFORM_KEY_TAB,
    PLATFORM_KEY_ENTER,PLATFORM_KEY_TAB,PLATFORM_KEY_ENTER,PLATFORM_KEY_TAB,
    PLATFORM_KEY_TAB,PLATFORM_KEY_TAB,PLATFORM_KEY_ENTER,
    PLATFORM_KEY_TAB,PLATFORM_KEY_TAB,PLATFORM_KEY_ENTER,PLATFORM_KEY_ESCAPE,
    PLATFORM_KEY_TAB,PLATFORM_KEY_TAB};
  if(frame==2) { *event=(InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint='x'}; return 1; }
  if(frame<=0 || frame>=(int)(sizeof(keys)/sizeof(keys[0]))) return 0;
  *event=(InputKeyEvent){.kind=INPUT_KEY,.key=keys[frame],.action=PLATFORM_PRESS,
                       .mods=(frame==21||frame==22||frame==28||frame==29)?PLATFORM_MOD_SHIFT:0};
  return 1;
}
bool ui_reference_navigation_result(const UIReferenceScene *s) {
  return !strcmp(s->text,"x") && !s->checked && fabsf(s->value-.51f)<.0001f &&
         s->choice==1 && s->light && s->clicks==2 && !s->dialog && !ui_file_picker_is_open("reference/picker") &&
         !strcmp(ui_keyboard_focus_id(),"reference/count");
}
void ui_reference_init(UIReferenceScene *s) {
  *s=(UIReferenceScene){.value=.5f,.checked=true};
  s->split_ratio=.5f; s->accent=ui_theme_dark().accent;
  snprintf(s->text,sizeof(s->text),"Caffè € — testo UTF-8");
}
static void reference_accent(UIReferenceScene *s) {
  UITheme theme=ui_get_theme();
  theme.accent=s->accent;
  theme.selected=(UIColor){theme.panel.r*.75f+s->accent.r*.25f,
    theme.panel.g*.75f+s->accent.g*.25f,theme.panel.b*.75f+s->accent.b*.25f};
  ui_set_theme(theme);
}
void ui_reference_draw(UIReferenceScene *s,Font *font,float width,float height,const char *root) {
  if(width<120||height<120) return;
  UIPanel panel=ui_panel_begin("reference","UI Framework — riferimento",font,
                               (UIRect){20,20,width-40,height-40},1);
  if(panel.clip_pushed) {
    UIScrollScope scroll=ui_scroll_begin("reference/scroll",panel.content,1);
    if(scroll.clip_pushed) {
      UIPropertyLayout p=ui_property_layout_begin(scroll.layout.bounds.x,
        scroll.layout.cursor_y,scroll.layout.bounds.w,1);
      if(!s->layout_demo) {
      ui_property_note(&p,font,"Stessa scena, backend indipendenti. Scorri per vedere tutti i controlli.");
      ui_property_text(&p,"reference/text",font,"Testo UTF-8",s->text,sizeof(s->text));
      ui_property_checkbox(&p,"reference/check",font,"Abilitato",&s->checked);
      ui_property_slider(&p,"reference/slider",font,"Valore",&s->value,0,1,NULL);
      const char *options[]={"Prima scelta","Seconda scelta","Terza scelta"};
      ui_property_dropdown(&p,"reference/select",font,"Selezione",options,3,&s->choice);
      if(ui_property_toggle(&p,"reference/theme",font,"Tema chiaro",&s->light)) {
        ui_set_theme(s->light?ui_theme_light():ui_theme_dark());
        if(s->custom_accent) reference_accent(s);
        else s->accent=ui_get_theme().accent;
      }
      if(ui_property_action(&p,"reference/count",font,"Incrementa",false)) ++s->clicks;
      char count[64]; snprintf(count,sizeof(count),"%d",s->clicks);
      ui_property_value(&p,font,"Click",count);
      if(ui_property_action(&p,"reference/dialog",font,"Apri dialogo",false)) s->dialog=true;
      if(ui_property_action(&p,"reference/file",font,"Scegli file",false)) {
        UIFilePickerRequest request={.mode=UI_FILE_PICKER_OPEN,.root_directory=root,.image_preview=true};
        ui_file_picker_open("reference/picker","Scegli un file",&request);
      }
      ui_property_value(&p,font,"Percorso",s->path[0]?s->path:"Nessun file selezionato");
      for(int i=0;i<12;++i) {
        char row[96]; snprintf(row,sizeof(row),"Riga %02d — contenuto per verificare scroll e clipping",i+1);
        ui_property_note(&p,font,row);
      }
      }
      const UITab tabs[]={{"Colonne",false},{"Righe",false},{"Non disponibile",true}};
      ui_tabs("reference/layout-tabs",font,(UIRect){p.x,p.cursor_y,p.width,32},tabs,3,&s->layout_tab);
      p.cursor_y+=40;
      UIRect first,second;
      ui_splitter("reference/layout-split",(UIRect){p.x,p.cursor_y,p.width,180},
        s->layout_tab==0?UI_SPLIT_COLUMNS:UI_SPLIT_ROWS,8,48,48,&s->split_ratio,&first,&second);
      UIPanel left=ui_panel_begin("reference/first","Primo pannello",font,first,1);
      if(left.clip_pushed) {
        UIPropertyLayout content=ui_property_layout_begin(left.content.x,left.content.y,left.content.w,1);
        ui_property_note(&content,font,"Trascina il separatore");
      }
      ui_panel_end(&left);
      UIPanel right=ui_panel_begin("reference/second","Secondo pannello",font,second,1);
      if(right.clip_pushed) {
        UIPropertyLayout content=ui_property_layout_begin(right.content.x,right.content.y,right.content.w,1);
        ui_property_note(&content,font,"Oppure Tab e frecce");
      }
      ui_panel_end(&right);
      p.cursor_y+=188;
      if(ui_property_action(&p,"reference/basic",font,"Widget basilari",false)) s->basic_dialog=true;
      if(ui_property_action(&p,"reference/options",font,"Opzioni aspetto",false)) s->options=true;
      ui_property_layout_commit(&scroll.layout,&p);
    }
    ui_scroll_end(&scroll);
  }
  ui_panel_end(&panel);
  if(s->dialog) {
    float x,y,w,h;
    if(ui_modal_begin("reference/modal","Dialogo",font,440,220,&s->dialog,&x,&y,&w,&h)) {
      UIPropertyLayout p=ui_property_layout_begin(x+12,y+12,w-24,1);
      ui_property_note(&p,font,"Dialogo modale: il contenuto sottostante non riceve input.");
      if(ui_property_action(&p,"reference/close",font,"Chiudi",false)) s->dialog=false;
    }
    ui_modal_end("reference/modal",s->dialog);
  }
  if(s->basic_dialog) {
    float x,y,w,h;
    if(ui_modal_begin("reference/basic-modal","Widget basilari",font,600,420,&s->basic_dialog,&x,&y,&w,&h)) {
      UIPropertyLayout p=ui_property_layout_begin(x+12,y+12,w-24,1);
      ui_property_note(&p,font,"Scegli una modalita'; soffermati sulle opzioni per l'aiuto.");
      const char *labels[]={"Locale","Remoto","Non disponibile"};
      for(int i=0;i<3;++i) {
        char id[64]; snprintf(id,sizeof(id),"reference/radio/%d",i);
        UIRect row={p.x,ui_property_layout_next_row(&p),p.width,p.row_height};
        ui_radio_button(id,font,row,labels[i],i,&s->radio_choice,i==2);
        ui_tooltip(id,font,row,i==0?"Elaborazione su questo computer.":"Elaborazione su un server remoto.",.5f);
      }
      ui_progress_bar(font,(UIRect){p.x,ui_property_layout_next_row(&p),p.width,p.row_height},s->value,false,0,NULL);
      ui_progress_bar(font,(UIRect){p.x,ui_property_layout_next_row(&p),p.width,p.row_height},0,true,
        (float)(ui_frame_serial_hint()%3600)/60,"Operazione in corso");
      ui_property_password(&p,"reference/password",font,"Password",s->password,sizeof(s->password),&s->password_revealed);
      if(ui_property_action(&p,"reference/basic-close",font,"Chiudi",false)) s->basic_dialog=false;
    }
    ui_modal_end("reference/basic-modal",s->basic_dialog);
  }
  if(s->options) {
    float x,y,w,h;
    if(ui_modal_begin("reference/options-modal","Opzioni aspetto",font,560,400,&s->options,&x,&y,&w,&h)) {
      UIPropertyLayout p=ui_property_layout_begin(x+12,y+12,w-24,1);
      if(ui_property_color(&p,"reference/accent",font,"Colore di accento",&s->accent)) {
        s->custom_accent=true; reference_accent(s);
      }
      if(ui_property_action(&p,"reference/reset-accent",font,"Ripristina colore predefinito",false)) {
        UITheme defaults=s->light?ui_theme_light():ui_theme_dark(),theme=ui_get_theme();
        theme.accent=defaults.accent; theme.selected=defaults.selected; ui_set_theme(theme);
        s->accent=defaults.accent; s->custom_accent=false;
      }
      if(ui_property_action(&p,"reference/options-close",font,"Chiudi",false)) s->options=false;
    }
    ui_modal_end("reference/options-modal",s->options);
  }
  ui_file_picker_draw(font,s->path,sizeof(s->path));
}

/* Selection/undo visual smoke: deliberately does not access the desktop clipboard. */
size_t ui_reference_editing_events(int frame,InputKeyEvent *event) {
  static const int keys[]={0,PLATFORM_KEY_TAB,0,0,0,PLATFORM_KEY_LEFT,0,
    PLATFORM_KEY_Z,PLATFORM_KEY_Z,PLATFORM_KEY_A,0,PLATFORM_KEY_Z,
    PLATFORM_KEY_RIGHT,PLATFORM_KEY_LEFT,PLATFORM_KEY_LEFT};
  if(frame<=0 || frame>=15) return 0;
  unsigned cp=frame==2?0xe8:frame==3?0x20ac:frame==4?0x1f642:frame==6?'x':frame==10?0x4e2d:0;
  *event=(InputKeyEvent){.kind=cp?INPUT_CHARACTER:INPUT_KEY,.codepoint=cp,
      .key=keys[frame],.action=PLATFORM_PRESS,
      .mods=(frame==5||frame==13||frame==14?PLATFORM_MOD_SHIFT:0)|
            (frame==7||frame==8||frame==9||frame==11?PLATFORM_MOD_CONTROL:0)|
            (frame==8?PLATFORM_MOD_SHIFT:0)};
  return 1;
}
bool ui_reference_editing_result(const UIReferenceScene *s) {
  return !strcmp(s->text,"è€x")&&!strcmp(ui_keyboard_focus_id(),"reference/text");
}

size_t ui_reference_layout_events(int frame,InputKeyEvent *event) {
  const int keys[]={0,PLATFORM_KEY_TAB,PLATFORM_KEY_RIGHT,PLATFORM_KEY_TAB,
                   PLATFORM_KEY_DOWN,PLATFORM_KEY_END,PLATFORM_KEY_HOME};
  if(frame<=0||frame>=7) return 0;
  *event=(InputKeyEvent){.kind=INPUT_KEY,.key=keys[frame],.action=PLATFORM_PRESS}; return 1;
}
bool ui_reference_layout_result(const UIReferenceScene *s) {
  return s->layout_tab==1 && fabsf(s->split_ratio-48.0f/172)<.0001f &&
         !strcmp(ui_keyboard_focus_id(),"reference/layout-split");
}

size_t ui_reference_color_events(int frame,InputKeyEvent *event) {
  if(frame<=0||frame>5) return 0;
  *event=(InputKeyEvent){.kind=INPUT_KEY,.key=frame==5?PLATFORM_KEY_ENTER:PLATFORM_KEY_TAB,.action=PLATFORM_PRESS}; return 1;
}
bool ui_reference_color_result(const UIReferenceScene *s) {
  UIColor expected=ui_color_srgb(0x48bd78),actual=ui_get_theme().accent;
  return s->custom_accent && fabsf(actual.r-expected.r)<.0001f &&
    fabsf(actual.g-expected.g)<.0001f && fabsf(actual.b-expected.b)<.0001f;
}

size_t ui_reference_basic_events(int frame,InputKeyEvent *event) {
  const int keys[]={0,PLATFORM_KEY_TAB,PLATFORM_KEY_TAB,PLATFORM_KEY_SPACE,
    PLATFORM_KEY_TAB,0,0,PLATFORM_KEY_TAB,PLATFORM_KEY_SPACE,PLATFORM_KEY_SPACE};
  if(frame<=0||frame>=10) return 0;
  unsigned cp=frame==5?0xe8:frame==6?0x20ac:0;
  *event=(InputKeyEvent){.kind=cp?INPUT_CHARACTER:INPUT_KEY,.codepoint=cp,.key=keys[frame],.action=PLATFORM_PRESS}; return 1;
}
bool ui_reference_basic_result(const UIReferenceScene *s) {
  return s->radio_choice==1 && !s->password_revealed && !strcmp(s->password,"è€");
}
