#define main navigation_fixture_main
#include "ui_navigation_test.c"
#undef main
#include "../src/ui/ui_state_menu_internal.h"
#include "../src/ui/ui_state_frame_internal.h"
#include "ui_storage_alloc_probe.h"
static void frame(float x,float y,bool down) {
  UIFrameInput input={.width=900,.height=700,.mouse_x=x,.mouse_y=y,.mouse_down=down,.delta_time=.016f};
  CHECK(ui_begin_frame_input(&input));
}
static const UIMenuItem root_item={.id="branch",.label="Sottomenu",.kind=UI_MENU_ITEM_SUBMENU,.submenu_id="child"};
static void copies_and_children(void) {
  UIContext *a=context(); CHECK(!storage_live);
  UIMenuHierarchy *menu=&ui_state_menu(a)->g_menu;
  char label[64]="Caffè €",shortcut[16]="Ctrl+Z";
  UIMenuItem item={.id="leaf",.label=label,.shortcut=shortcut};
  frame(-20,-20,false);
  ui_menu_open_context("root",100,80,220,&root_item,1);
  CHECK(ui_menu_submit_submenu("root","branch","child",&item,1));
  strcpy(label,"changed"); strcpy(shortcut,"mutated");
  CHECK(!strcmp(menu->submissions[0].items[0].label,"Caffè €"));
  CHECK(!strcmp(menu->submissions[0].items[0].shortcut,"Ctrl+Z"));
  ui_menu_render_overlay(font); ui_end_frame();
  UIMenuLevel *root=&menu->levels[0];
  float x=root->popup.x+10,y=root->popup.y+10;
  frame(x,y,false); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(menu->depth==2&&!strcmp(menu->levels[1].items[0].label,"Caffè €"));
  CHECK(menu->levels[1].items!=menu->submissions[0].items);
  size_t calls=storage_requests,bytes=storage_live;
  ui_menu_close_hierarchy();
  storage_fail_after=0;
  frame(-20,-20,false); ui_menu_open_context("root",100,80,220,&root_item,1);
  CHECK(ui_menu_submit_submenu("root","branch","child",&item,1));
  ui_menu_render_overlay(font); ui_end_frame();
  frame(x,y,false); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(storage_requests==calls&&storage_fail_after==0&&storage_live==bytes); storage_fail_after=-1;
  UIContext *b=context();
  frame(-20,-20,false); ui_menu_open_context("root",100,80,220,&item,1); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(ui_state_menu(b)->g_menu.levels[0].items!=root->items);
  CHECK(ui_context_reset(a));
  CHECK(storage_live==(size_t)ui_state_menu(b)->g_menu.levels[0].item_capacity*sizeof(UIStoredMenuItem));
  CHECK(ui_context_destroy(a)&&ui_context_destroy(b)&&!storage_live);
}
static void failures(void) {
  UIContext *ctx=context(); UIMenuHierarchy *menu=&ui_state_menu(ctx)->g_menu;
  frame(110,90,true); storage_fail_after=0;
  ui_menu_open_context("root",100,80,220,&root_item,1); storage_fail_after=-1;
  CHECK(!menu->depth&&menu->overflowed&&!storage_live&&ui_state_frame(ctx)->g_click_claimed_by_menu);
  CHECK(!button("under",80,false)); ui_end_frame();
  frame(-20,-20,false); ui_menu_open_context("root",100,80,220,&root_item,1);
  UIMenuItem leaf={.id="leaf",.label="Foglia"}; storage_fail_after=0;
  CHECK(!ui_menu_submit_submenu("root","branch","child",&leaf,1)); storage_fail_after=-1;
  CHECK(!menu->depth&&!menu->submission_count); ui_end_frame();
  frame(-20,-20,false); ui_menu_open_context("root",100,80,220,&root_item,1);
  CHECK(ui_menu_submit_submenu("root","branch","child",&leaf,1));
  ui_menu_render_overlay(font); ui_end_frame();
  UIMenuLevel *root=&menu->levels[0];
  frame(root->popup.x+10,root->popup.y+10,false); storage_fail_after=0;
  ui_menu_render_overlay(font); storage_fail_after=-1; ui_end_frame();
  CHECK(menu->depth==1&&!menu->levels[1].items); /* Parent survives a failed child. */
  frame(root->popup.x+10,root->popup.y+10,false); ui_menu_render_overlay(font); ui_end_frame();
  CHECK(menu->depth==2&&menu->levels[1].items);
  /* Growing an existing submission can fail without losing its old buffer. */
  UIMenuItem items[32]; for(int i=0;i<32;++i) items[i]=leaf;
  UIStoredMenuItem *old=menu->submissions[0].items; size_t bytes=storage_live;
  frame(-20,-20,false); storage_fail_after=0;
  CHECK(!ui_menu_submit_submenu("root","branch","child",items,32)); storage_fail_after=-1;
  CHECK(menu->submissions[0].items==old&&storage_live==bytes&&!menu->depth); ui_end_frame();
  frame(-20,-20,false); storage_force_move=true;
  CHECK(ui_menu_submit_submenu("root","branch","child",items,32));
  CHECK(menu->submissions[0].items!=old&&menu->submissions[0].item_capacity==32); ui_end_frame();
  CHECK(ui_context_destroy(ctx)&&!storage_live);
}
static void bounds_and_bar_growth(void) {
  UIContext *ctx=context(); UIMenuHierarchy *menu=&ui_state_menu(ctx)->g_menu;
  UIMenuItem items[33]; for(int i=0;i<33;++i) items[i]=(UIMenuItem){.id="leaf",.label="Voce"};
  frame(-20,-20,false); ui_menu_open_context("root",100,80,220,items,1); ui_end_frame();
  UIStoredMenuItem *old=menu->levels[0].items;
  frame(-20,-20,false); storage_fail_after=0;
  ui_menu_bar("root","File",font,20,20,220,14,items,32,NULL); storage_fail_after=-1;
  CHECK(!menu->depth&&menu->levels[0].items==old); ui_end_frame();
  frame(-20,-20,false); storage_force_move=true;
  ui_menu_open_context("root",100,80,220,items,32);
  CHECK(menu->depth==1&&menu->levels[0].items!=old&&menu->levels[0].item_capacity==32);
  ui_menu_bar("root","File",font,20,20,220,14,items,33,NULL);
  CHECK(menu->levels[0].item_count==32&&menu->overflowed); ui_end_frame();
  for(int i=0;i<MAX_UI_SUBMENU_SUBMISSIONS;++i) {
    char parent[32],child[32]; snprintf(parent,sizeof(parent),"branch-%d",i); snprintf(child,sizeof(child),"child-%d",i);
    CHECK(ui_menu_submit_submenu("root",parent,child,items,1));
  }
  size_t bytes=storage_live;
  CHECK(!ui_menu_submit_submenu("root","overflow","overflow",items,1));
  CHECK(menu->submission_count==MAX_UI_SUBMENU_SUBMISSIONS&&storage_live==bytes);
  CHECK(ui_context_destroy(ctx)&&!storage_live);
}
int main(void) {
  copies_and_children(); failures(); bounds_and_bar_growth(); CHECK(!clips&&!ui_context_get_current());
  puts("Menu storage: PASS (lazy root/submissions/children, copies, warm reuse, OOM, stable levels, contexts, cleanup)");
}
