#define _POSIX_C_SOURCE 200809L
#define main navigation_fixture_main
#include "ui_navigation_test.c"
#undef main
#include "ui/draw_list.h"
#include "../tools/ui_benchmark_metrics.h"
#include "../src/ui/ui_state_buttons_internal.h"
#include "../src/ui/ui_state_dropdowns_internal.h"
#include "../src/ui/ui_state_text_fields_internal.h"
static unsigned random_next(unsigned *state) { *state=*state*1664525u+1013904223u; return *state; }
int main(void) {
  const unsigned seeds[]={1,0x12345678u,0xc0ffeeu}; size_t frames=0,peak=0;
  for(size_t seed=0;seed<3;++seed) {
    unsigned rng=seeds[seed];
    UIContext *contexts[]={context(),context()};
    size_t baseline=bench_metrics.live;
    for(int epoch=0;epoch<12;++epoch) {
      UIDrawList *lists[2]={NULL,NULL}; char buffers[2][64]={{0},{0}};
      for(int c=0;c<2;++c) {
        CHECK(ui_context_set_current(contexts[c]));
        if(epoch%2) { lists[c]=ui_draw_list_create(&renderer,NULL); CHECK(lists[c]); }
        CHECK(ui_context_set_renderer(contexts[c],lists[c]?ui_draw_list_renderer():&renderer,lists[c]));
      }
      for(int step=0;step<768;++step) {
        int c=step%2; UIContext *ctx=contexts[c]; CHECK(ui_context_set_current(ctx));
        if(lists[c]) CHECK(ui_draw_list_begin(lists[c]));
        unsigned choice=random_next(&rng);
        InputKeyEvent events[]={character(0xe8),key(PLATFORM_KEY_BACKSPACE,0),key(PLATFORM_KEY_TAB,0)};
        UIFrameInput input={.width=640,.height=480,.mouse_x=-20,.mouse_y=-20,.delta_time=.016f,
          .focus_lost=step%37==0,.events=events,.event_count=choice%4};
        CHECK(ui_begin_frame_input_status(&input)==UI_ERROR_NONE);
        char id[64];
        for(int i=0;i<4;++i) { snprintf(id,sizeof(id),"screen-%d-button-%d-%d",epoch,step,i); button(id,20+i*32,false); }
        snprintf(id,sizeof(id),"edit-%d",step/8); field(id,170,buffers[c]);
        if(step%8==c) ui_text_input_focus(id);
        snprintf(id,sizeof(id),"choice-%d",step/2);
        const char *options[]={"Caffè","€","three"}; int selected=0;
        if(choice%11==0) ui_dropdown_set_open(id,true);
        ui_dropdown(id,font,ANCHOR_TOP_LEFT,0,14,250,200,180,options,3,&selected,NULL,NULL);
        if(choice%7==0) {
          char label[64]; snprintf(label,sizeof(label),"Voce %u",choice);
          UIMenuItem item={.id="leaf",.label=label};
          ui_menu_open_context("session-root",350,250,180,&item,1);
          CHECK(ui_menu_submit_submenu("session-root","leaf","child",&item,1));
          memset(label,'X',strlen(label)); /* menu must own its strings */
        }
        ui_menu_render_overlay(font); ui_end_frame();
        if(lists[c]) CHECK(ui_draw_list_replay(lists[c]));
        CHECK(!clips&&ui_context_error(ctx).code==UI_ERROR_NONE);
        CHECK(ui_state_buttons(ctx)->ui_state_count<=MAX_UI_ELEMENTS);
        CHECK(ui_state_dropdowns(ctx)->dropdown_state_count<=MAX_UI_DROPDOWNS);
        CHECK(ui_state_text_fields(ctx)->text_input_state_count<=MAX_UI_TEXT_INPUTS);
        for(int i=0;i<3;++i) CHECK(!events[i].consumed);
        ++frames;
      }
      for(int c=0;c<2;++c) {
        CHECK(ui_context_set_current(contexts[c]));
        CHECK(ui_context_set_renderer(contexts[c],&renderer,NULL));
        ui_draw_list_destroy(lists[c]);
        CHECK(ui_context_reset(contexts[c]));
        CHECK(!ui_has_pointer_capture()&&!ui_has_keyboard_capture());
        CHECK(!ui_state_buttons(contexts[c])->ui_state_count&&!ui_state_dropdowns(contexts[c])->dropdown_state_count);
      }
      CHECK(bench_metrics.live==baseline&&!bench_metrics.unknown_frees);
    }
    CHECK(ui_context_destroy(contexts[0])&&ui_context_destroy(contexts[1]));
    CHECK(!bench_metrics.live&&!bench_metrics.unknown_frees);
    if(bench_metrics.peak>peak) peak=bench_metrics.peak;
    bench_reset_metrics();
  }
  printf("UI prolonged sessions: PASS (3 seeds, %zu frames, 36 reset epochs, immediate/deferred, bounded caches, peak=%zu, final live=0)\n",frames,peak);
}
