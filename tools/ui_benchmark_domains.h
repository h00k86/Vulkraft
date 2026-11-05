#pragma once
/* Inspect individual private domains; never import the physical UIContext. */
#include "../src/ui/ui_state_navigation_internal.h"
#include "../src/ui/ui_state_render_internal.h"
#include "../src/ui/ui_state_clipboard_internal.h"
#include "../src/ui/ui_state_tooltip_internal.h"
#include "../src/ui/ui_state_frame_internal.h"
#include "../src/ui/ui_state_ids_internal.h"
#include "../src/ui/ui_state_panels_internal.h"
#include "../src/ui/ui_state_buttons_internal.h"
#include "../src/ui/ui_state_modals_internal.h"
#include "../src/ui/ui_state_style_internal.h"
#include "../src/ui/ui_state_menu_internal.h"
#include "../src/ui/ui_state_numeric_internal.h"
#include "../src/ui/ui_state_sliders_internal.h"
#include "../src/ui/ui_state_text_fields_internal.h"
#include "../src/ui/ui_state_dropdowns_internal.h"
#include "../src/ui/ui_state_picker_internal.h"
#include "../src/ui/ui_state_debug_internal.h"
#include "../src/ui/ui_state_errors_internal.h"
static size_t bench_domains(void) {
  struct { const char *name; size_t bytes; } domains[]={
    {"navigation",sizeof(UINavigationState)}, {"render",sizeof(UIRenderState)},
    {"clipboard",sizeof(UIClipboardState)}, {"tooltip",sizeof(UITooltipState)},
    {"frame",sizeof(UIFrameState)}, {"ids",sizeof(UIIdsState)},
    {"panels",sizeof(UIPanelsState)}, {"buttons",sizeof(UIButtonsState)},
    {"modals",sizeof(UIModalsState)}, {"style",sizeof(UIStyleState)},
    {"menu",sizeof(UIMenuState)}, {"numeric",sizeof(UINumericState)},
    {"sliders",sizeof(UISlidersState)}, {"text_fields",sizeof(UITextFieldsState)},
    {"dropdowns",sizeof(UIDropdownsState)}, {"picker",sizeof(UIPickerState)},
    {"debug",sizeof(UIDebugState)}, {"errors",sizeof(UIErrorsState)}
  };
  size_t total=0;
  printf("\"domains\":[");
  for (size_t i=0;i<sizeof(domains)/sizeof(domains[0]);++i) {
    printf("%s{\"name\":\"%s\",\"fixed_bytes\":%zu}",i?",":"",domains[i].name,domains[i].bytes);
    total+=domains[i].bytes;
  }
  printf("],\"domain_total_bytes\":%zu",total);
  printf(",\"fixed_buffers\":{\"navigation_previous\":%zu,\"navigation_current\":%zu,"
         "\"navigation_event_owners\":%zu,\"menu_levels\":%zu,\"menu_submissions\":%zu,"
         "\"dropdown_option_copies\":0,\"dropdown_option_pointers\":%zu}",
         sizeof(((UINavigationState *)0)->nav_previous),sizeof(((UINavigationState *)0)->nav_current),
         sizeof(((UINavigationState *)0)->nav_event_owner),sizeof(((UIMenuHierarchy *)0)->levels),
         sizeof(((UIMenuHierarchy *)0)->submissions),sizeof(((UIDropdownState *)0)->popupOptions)*MAX_UI_DROPDOWNS);
  return total;
}
