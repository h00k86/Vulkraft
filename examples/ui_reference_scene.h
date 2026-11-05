#pragma once
#include "ui/ui.h"
typedef struct {
  char text[256],path[512];
  bool components;
  int component_tab,color_mode,list_selection,tree_selection;
  char search[128];
  UIColorRGBA rgba;
  UITreeNode nodes[5];
  float value,split_ratio;
  int layout_tab;
  int choice,clicks;
  UIColor accent;
  bool options,custom_accent,basic_dialog,password_revealed;
  int radio_choice;
  char password[128];
  bool checked,dialog,light,layout_demo;
} UIReferenceScene;
void ui_reference_init(UIReferenceScene *scene);
void ui_reference_draw(UIReferenceScene *scene,Font *font,float width,float height,const char *root);
/* Deterministic smoke input shared by both hosts; no desktop event injection. */
size_t ui_reference_navigation_events(int frame,InputKeyEvent *event);
bool ui_reference_navigation_result(const UIReferenceScene *scene);

size_t ui_reference_editing_events(int frame,InputKeyEvent *event);
bool ui_reference_editing_result(const UIReferenceScene *scene);

size_t ui_reference_layout_events(int frame,InputKeyEvent *event);
bool ui_reference_layout_result(const UIReferenceScene *scene);

size_t ui_reference_color_events(int frame,InputKeyEvent *event);
bool ui_reference_color_result(const UIReferenceScene *scene);

size_t ui_reference_basic_events(int frame,InputKeyEvent *event);
bool ui_reference_basic_result(const UIReferenceScene *scene);
