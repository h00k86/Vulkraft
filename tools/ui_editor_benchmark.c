/* Real config-editor composition, scripted input and simulated renderer.
 * The tool owns temporary fixtures and never opens a native window. */
#define _POSIX_C_SOURCE 200809L
#include "ui/ui.h"
#include "ui/draw_list.h"
#include "ui/file_picker.h"
#include "../examples/config_editor/view.h"
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "ui_benchmark_metrics.h"
#include "ui_benchmark_domains.h"
#include "../src/ui/ui_navigation_internal.h"
#include "../src/ui/ui_file_picker_internal.h"
struct Font { int unused; };
static Font font;
static size_t draws, texts;
static size_t expected_draws[7],expected_texts[7];
static int depth;
static void quad(void *u,float x,float y,float w,float h,float r,float g,float b) { ++draws; }
static void alpha(void *u,float x,float y,float w,float h,float r,float g,float b,float a) { ++draws; }
static void text(void *u,Font *f,const char *s,float x,float y,float z,float r,float g,float b) { ++draws; ++texts; }
static void image(void *u,float x,float y,float w,float h,int s,float a,float b,float c,float d) { ++draws; }
static float ascent(void *u,Font *f,float s) { return s; }
static float measure(void *u,Font *f,const char *s,float z) { return strlen(s)*z*.55f; }
static bool push(void *u,float x,float y,float w,float h) { ++depth; return true; }
static bool pop(void *u) { if (!depth) return false; --depth; return true; }
static int clip_depth(void *u) { return depth; }
static const UIRenderer renderer={.draw_quad=quad,.draw_quad_alpha=alpha,
  .draw_text_colored=text,.draw_textured_quad=image,.font_ascent=ascent,
  .measure_text_width=measure,.text_clip_push=push,.text_clip_pop=pop,.text_clip_depth=clip_depth};
static const UIMenuItem menu_items[]={
  {.id="open",.label="Apri configurazione"}, {.id="save",.label="Salva"},
  {.id="options",.label="Opzioni",.kind=UI_MENU_ITEM_SUBMENU,.submenu_id="options"}
};
static const UIMenuItem submenu_items[]={{.id="dark",.label="Tema scuro"},{.id="light",.label="Tema chiaro"}};
enum { IDLE, EDIT, SCROLL, MENU, PICKER, REOPEN_PICKER };
enum { WARMUP=20, SAMPLES=200 };
static void draw_frame(ConfigEditor *editor,UIDrawList *list,int scenario,int serial) {
  InputKeyEvent event={0};
  UIFrameInput input={.width=1000,.height=800,.delta_time=1.f/60,.mouse_x=-1,.mouse_y=-1};
  if (scenario==EDIT) {
    int keys[]={0,PLATFORM_KEY_Z,PLATFORM_KEY_Y,PLATFORM_KEY_BACKSPACE};
    int step=serial%4;
    event=step?(InputKeyEvent){.kind=INPUT_KEY,.key=keys[step],.action=PLATFORM_PRESS,
                              .mods=step<3?PLATFORM_MOD_CONTROL:0}:
               (InputKeyEvent){.kind=INPUT_CHARACTER,.codepoint=0xe8};
    input.events=&event; input.event_count=1;
  }
  if (scenario==SCROLL) {
    input.mouse_x=400; input.mouse_y=250;
    input.scroll_delta=(serial%40<20)?-1:1;
  }
  if (scenario==REOPEN_PICKER) {
    ui_file_picker_cancel(); config_editor_cancel_picker(editor);
    config_editor_request(editor,CONFIG_ACTION_OPEN);
  }
  /* Variable labels/notes are application-owned, just like external updates. */
  if (scenario==SCROLL||scenario==MENU) {
    snprintf(editor->document.items[0].note,CONFIG_NOTE_CAP,"Fotogramma %d - UTF-8: è €",serial);
    snprintf(editor->document.items[editor->document.count-1].name,CONFIG_NAME_CAP,"Profilo %d",serial);
  }
  if (list) bench_require(ui_draw_list_begin(list));
  bench_require(ui_begin_frame_input(&input));
  config_editor_draw(editor,&font,1000,800);
  if (scenario==MENU) {
    if (!ui_menu_hierarchy_is_open()) ui_menu_open_context("bench/menu",600,80,240,menu_items,3);
    bench_require(ui_menu_submit_submenu("bench/menu","options","options",submenu_items,2));
    ui_menu_render_overlay(&font);
  }
  ui_draw_dropdown_popups(); ui_end_frame();
  if (list) bench_require(ui_draw_list_replay(list));
  bench_require(depth==0);
}
static size_t memory_usage(UIContext *ctx) {
  UITextFieldsState *fields=ui_state_text_fields(ctx);
  size_t revisions=0, history=0, snapshots=0;
  size_t dropdown_bytes=0;
  UIDropdownsState *dropdowns=ui_state_dropdowns(ctx);
  for(int i=0;i<dropdowns->dropdown_state_count;++i)
    dropdown_bytes+=(size_t)dropdowns->dropdown_states[i].popupOptionCapacity*sizeof(*dropdowns->dropdown_states[i].popupOptions);
  UIMenuHierarchy *menu=&ui_state_menu(ctx)->g_menu; size_t menu_bytes=0;
  for(int i=0;i<UI_MENU_MAX_DEPTH;++i) menu_bytes+=(size_t)menu->levels[i].item_capacity*sizeof(UIStoredMenuItem);
  for(int i=0;i<MAX_UI_SUBMENU_SUBMISSIONS;++i) menu_bytes+=(size_t)menu->submissions[i].item_capacity*sizeof(UIStoredMenuItem);
  size_t owner_bytes=ui_state_navigation(ctx)->nav_event_capacity*UI_ID_CAPACITY;
  for (int i=0;i<fields->text_input_state_count;++i) {
    UITextInputState *s=&fields->text_input_states[i];
    history+=s->history_bytes; revisions+=s->history_count;
    if (s->original_long) snapshots+=strlen(s->original_long)+1;
  }
  printf(",\"registry_usage\":{\"buttons\":%d,\"buttons_capacity\":%d,\"navigation\":%d,"
         "\"navigation_capacity\":%d,\"text_fields\":%d,\"text_fields_capacity\":%d,"
         "\"dropdowns\":%d,\"dropdowns_capacity\":%d,\"numeric\":%d,\"numeric_capacity\":%d,"
         "\"menu_depth\":%d,\"menu_submissions\":%d},\"text_history_bytes\":%zu,"
         "\"text_revisions\":%zu,\"long_snapshot_bytes\":%zu,\"picker_heap_bytes\":%zu",
         ui_state_buttons(ctx)->ui_state_count,MAX_UI_ELEMENTS,ui_state_navigation(ctx)->nav_count,MAX_UI_ELEMENTS,
         fields->text_input_state_count,MAX_UI_TEXT_INPUTS,ui_state_dropdowns(ctx)->dropdown_state_count,MAX_UI_DROPDOWNS,
         ui_state_numeric(ctx)->float_field_state_count,MAX_UI_FLOAT_FIELDS,
         ui_state_menu(ctx)->g_menu.depth,ui_state_menu(ctx)->g_menu.submission_count,
         history,revisions,snapshots,ui_state_picker(ctx)->file_picker_state?sizeof(UIFilePickerState):0);
  /* Reserved versus populated slots, not a claim that all spare capacity can
   * be removed: registries deliberately retain entries between frames. */
  printf(",\"registry_slots\":{\"buttons_reserved_bytes\":%zu,\"buttons_populated_bytes\":%zu,"
         "\"navigation_reserved_bytes\":%zu,\"navigation_declared_bytes\":%zu,"
         "\"text_fields_reserved_bytes\":%zu,\"text_fields_populated_bytes\":%zu,"
         "\"dropdowns_reserved_bytes\":%zu,\"dropdowns_populated_bytes\":%zu}",
         sizeof(((UIButtonsState *)0)->ui_states),(size_t)ui_state_buttons(ctx)->ui_state_count*sizeof(UIState),
         sizeof(((UINavigationState *)0)->nav_previous)+sizeof(((UINavigationState *)0)->nav_current),
         (size_t)(ui_state_navigation(ctx)->nav_count+ui_state_navigation(ctx)->nav_previous_count)*sizeof(UINavigationEntry),
         sizeof(fields->text_input_states),(size_t)fields->text_input_state_count*sizeof(UITextInputState),
         sizeof(((UIDropdownsState *)0)->dropdown_states),(size_t)ui_state_dropdowns(ctx)->dropdown_state_count*sizeof(UIDropdownState));
  printf(",\"dropdown_options_heap_bytes\":%zu",dropdown_bytes);
  printf(",\"menu_items_heap_bytes\":%zu,\"navigation_owners_heap_bytes\":%zu",menu_bytes,owner_bytes);
  return history+snapshots+dropdown_bytes+menu_bytes+owner_bytes+(ui_state_picker(ctx)->file_picker_state?sizeof(UIFilePickerState):0);
}
static void run_case(const char *root,int mode,int scenario,int profiles,bool comma) {
  bench_require(!bench_metrics.live);
  bench_reset_metrics();
  UIContext *ctx=ui_context_create(); bench_require(ctx&&ui_context_set_current(ctx));
  size_t context_bytes=bench_metrics.live;
  ConfigEditor *editor=calloc(1,sizeof(*editor)); bench_require(editor&&config_editor_init(editor,root));
  while (editor->document.count<profiles) bench_require(config_document_add(&editor->document));
  /* Initial population is fixture setup, not a dirty document. */
  editor->saved=editor->document;
  editor->history[0]=(ConfigSnapshot){editor->document,0};
  UIDrawList *list=mode?ui_draw_list_create(&renderer,NULL):NULL; bench_require(!mode||list);
  bench_require(ui_context_set_renderer(ctx,list?ui_draw_list_renderer():&renderer,list));
  ui_state_debug(ctx)->debug_enabled=0;
  UITheme theme=ui_get_theme(); theme.paper_panels=false; ui_set_theme(theme);
  draw_frame(editor,list,IDLE,0);
  if (scenario==EDIT) ui_text_input_focus("config/1/1/name");
  if (scenario==PICKER||scenario==REOPEN_PICKER) {
    editor->document=editor->saved;
    config_editor_request(editor,CONFIG_ACTION_OPEN);
  }
  size_t cold_live=bench_metrics.live;
  size_t cold_requests=bench_metrics.requests, cold_requested=bench_metrics.requested_bytes;
  for (int i=0;i<WARMUP;++i) draw_frame(editor,list,scenario,i);
  size_t warmup_requests=bench_metrics.requests-cold_requests;
  size_t warmup_requested=bench_metrics.requested_bytes-cold_requested;
  size_t before_live=bench_metrics.live; bench_reset_metrics(); draws=texts=0;
  double samples[SAMPLES];
  float scroll_min=1e30f,scroll_max=-1e30f;
  for (int i=0;i<SAMPLES;++i) {
    double start=bench_now(); draw_frame(editor,list,scenario,WARMUP+i); samples[i]=(bench_now()-start)*1e6;
    if (scenario==EDIT) {
      bool accented=i%4==0||i%4==2;
      bench_require(!strcmp(editor->document.items[0].name,accented?"Profilo 1è":"Profilo 1"));
    }
    if (scenario==SCROLL) {
      UIScrollDiagnostics scroll; bench_require(ui_scroll_get_diagnostics("config/scroll",&scroll));
      if (scroll.offset<scroll_min) scroll_min=scroll.offset;
      if (scroll.offset>scroll_max) scroll_max=scroll.offset;
    }
  }
  BenchMetrics metrics=bench_metrics;
  int case_index=scenario==IDLE?(profiles==1?0:1):scenario+1;
  if (!mode) { expected_draws[case_index]=draws; expected_texts[case_index]=texts; }
  else bench_require(draws==expected_draws[case_index]&&texts==expected_texts[case_index]);
  double total=0; for (int i=0;i<SAMPLES;++i) total+=samples[i];
  qsort(samples,SAMPLES,sizeof(double),bench_compare);
  if (scenario==EDIT) bench_require(!strcmp(editor->document.items[0].name,"Profilo 1"));
  if (scenario==SCROLL) {
    UIScrollDiagnostics scroll; bench_require(ui_scroll_get_diagnostics("config/scroll",&scroll));
    bench_require(scroll.content_height>scroll.viewport_height);
    bench_require(scroll_max>scroll_min);
  }
  if (scenario==MENU) bench_require(ui_menu_hierarchy_is_open());
  if (scenario==PICKER||scenario==REOPEN_PICKER) {
    bench_require(ui_file_picker_is_open("config/picker"));
    UIFilePickerState *picker=ui_state_picker(ctx)->file_picker_state;
    bench_require(picker&&picker->entry_count==32);
  }
  const char *names[]={"idle","editing_utf8_undo_redo","scroll_variable_labels","menu_overlay","picker_cached","picker_reopen_scan"};
  printf("%s{\"scenario\":\"%s\",\"mode\":\"%s\",\"profiles\":%d,\"mean_us\":%.3f,"
         "\"median_us\":%.3f,\"p95_us\":%.3f,\"context_bytes\":%zu,\"application_state_bytes\":%zu,"
         "\"cold_live_bytes\":%zu,\"cold_allocation_requests\":%zu,\"cold_requested_bytes\":%zu,"
         "\"warmup_allocation_requests\":%zu,\"warmup_requested_bytes\":%zu,"
         "\"live_before_frames\":%zu,\"live_after_frames\":%zu,\"peak_live_bytes\":%zu,"
         "\"allocation_requests_per_frame\":%.3f,\"free_calls_per_frame\":%.3f,"
         "\"requested_bytes_per_frame\":%.3f,\"draw_callbacks_per_frame\":%.3f,\"text_callbacks_per_frame\":%.3f",
         comma?",":"",names[scenario],mode?"deferred":"immediate",profiles,total/SAMPLES,samples[SAMPLES/2],samples[SAMPLES*95/100-1],
         context_bytes,sizeof(*editor),cold_live,cold_requests,cold_requested,warmup_requests,warmup_requested,before_live,metrics.live,metrics.peak,
         (double)metrics.requests/SAMPLES,(double)metrics.frees/SAMPLES,(double)metrics.requested_bytes/SAMPLES,
         (double)draws/SAMPLES,(double)texts/SAMPLES);
  size_t dynamic=memory_usage(ctx);
  bench_require(metrics.live>=context_bytes+sizeof(*editor)+dynamic);
  size_t remaining=metrics.live-context_bytes-sizeof(*editor)-dynamic;
  bench_require(mode?remaining>0:remaining==0);
  printf(",\"context_dynamic_heap_bytes\":%zu,\"draw_list_heap_by_difference\":%zu",dynamic,remaining);
  if (scenario==EDIT) printf(",\"verified_edit_cycles\":%d",SAMPLES/4);
  if (scenario==SCROLL) printf(",\"scroll_offset_min\":%.3f,\"scroll_offset_max\":%.3f",scroll_min,scroll_max);
  /* Independent successful-ID probes; they do not establish frame attribution. */
  UIButtonsState *buttons=ui_state_buttons(ctx);
  UIState *volatile sink=NULL; double start=bench_now();
  for (int pass=0;pass<500;++pass) for (int i=0;i<buttons->ui_state_count;++i) sink=ui_button_state(buttons->ui_states[i].id);
  double button_ns=buttons->ui_state_count?(bench_now()-start)*1e9/(500*buttons->ui_state_count):0;
  if (buttons->ui_state_count) bench_require(sink!=NULL);
  UINavigationState *nav=ui_state_navigation(ctx); volatile int found=-1; start=bench_now();
  for (int pass=0;pass<500;++pass) for (int i=0;i<nav->nav_count;++i) found=ui_nav_find(ctx,nav->nav_current,nav->nav_count,nav->nav_current[i].id);
  double nav_ns=nav->nav_count?(bench_now()-start)*1e9/(500*nav->nav_count):0;
  if (nav->nav_count) bench_require(found>=0);
  bench_require(ui_context_destroy(ctx)); ui_draw_list_destroy(list); free(editor);
  bench_require(!bench_metrics.live && !bench_metrics.unknown_frees);
  printf(",\"button_lookup_ns\":%.3f,\"navigation_lookup_ns\":%.3f,\"live_after_destroy\":0}",button_ns,nav_ns);
}
int main(void) {
  /* Observer self-check: realloc replaces the previous live byte count. */
  char *probe=malloc(16); bench_require(probe&&bench_metrics.live==16);
  probe=realloc(probe,32); bench_require(probe&&bench_metrics.live==32);
  free(probe); bench_require(!bench_metrics.live);
  char root[]="/tmp/ui-editor-benchmark-XXXXXX"; bench_require(mkdtemp(root)!=NULL);
  for (int i=0;i<32;++i) {
    char path[256]; snprintf(path,sizeof(path),"%s/profile-%02d.cfgapp",root,i);
    FILE *file=fopen(path,"w"); bench_require(file!=NULL); bench_require(fputs("fixture\n",file)>=0&&fclose(file)==0);
  }
  printf("{\"scope\":\"real editor composition, simulated renderer, instrumented requested heap only\","
         "\"observer_metadata_bytes\":%zu,"
         "\"warmup_frames\":%d,\"sample_frames\":%d,\"memory\":{",sizeof(bench_heap)+sizeof(bench_metrics),WARMUP,SAMPLES);
  size_t domains=bench_domains();
  bench_reset_metrics(); UIContext *ctx=ui_context_create(); bench_require(ctx!=NULL);
  size_t context_bytes=bench_metrics.live; bench_require(domains<=context_bytes);
  printf(",\"context_requested_bytes\":%zu,\"context_padding_bytes\":%zu},\"cases\":[",context_bytes,context_bytes-domains);
  bench_require(ui_context_destroy(ctx));
  for (int mode=0;mode<2;++mode) {
    run_case(root,mode,IDLE,1,mode!=0);
    run_case(root,mode,IDLE,32,true);
    run_case(root,mode,EDIT,1,true);
    run_case(root,mode,SCROLL,32,true);
    run_case(root,mode,MENU,32,true);
    run_case(root,mode,PICKER,32,true);
    run_case(root,mode,REOPEN_PICKER,32,true);
  }
  for (int i=0;i<32;++i) {
    char path[256]; snprintf(path,sizeof(path),"%s/profile-%02d.cfgapp",root,i); bench_require(unlink(path)==0);
  }
  bench_require(rmdir(root)==0); puts("]}"); return 0;
}
