#include "ui_text_edit_internal.h"
#include "text/utf8.h"
#include <stdlib.h>
#include <string.h>

/* Selection offsets are UTF-8 byte boundaries. Snapshots are owned by the
 * field, bounded both by count and total bytes. No context or drawing here. */
static uint64_t ui_text_hash(const char *text) {
  uint64_t hash=UINT64_C(14695981039346656037);
  for(const unsigned char *p=(const unsigned char *)text;*p;++p) {
    hash^=*p; hash*=UINT64_C(1099511628211);
  }
  return hash;
}
void ui_text_history_clear(UITextInputState *s) {
  for (int i=0;i<s->history_count;++i) free(s->history[i].text);
  s->history_count=0; s->history_position=0; s->history_bytes=0;
}
static void ui_text_history_drop(UITextInputState *s,int i) {
  s->history_bytes-=strlen(s->history[i].text)+1;
  free(s->history[i].text);
  memmove(s->history+i,s->history+i+1,(size_t)(s->history_count-i-1)*sizeof(*s->history));
  --s->history_count;
}
static void ui_text_history_save(UITextInputState *s,const char *buffer) {
  size_t n=strlen(buffer)+1;
  if(n>UI_TEXT_SNAPSHOT_BYTES) { ui_text_history_clear(s); return; }
  if(s->history_count && !strcmp(s->history[s->history_position].text,buffer)) {
    s->history[s->history_position].caret=s->caret;
    s->history[s->history_position].anchor=s->selection_anchor;
    return;
  }
  char *copy=malloc(n);
  if(!copy) { ui_text_history_clear(s); return; }
  memcpy(copy,buffer,n);
  while(s->history_count>s->history_position+1) ui_text_history_drop(s,s->history_count-1);
  while(s->history_count && (s->history_count==UI_TEXT_HISTORY_COUNT || s->history_bytes+n>UI_TEXT_HISTORY_BYTES))
    ui_text_history_drop(s,0);
  s->history[s->history_count++]=(UITextRevision){copy,s->caret,s->selection_anchor};
  s->history_bytes+=n; s->history_position=s->history_count-1;
}
static void ui_text_history_move(UITextInputState *s,char *buffer,int capacity,int step) {
  int next=s->history_position+step;
  if(next<0 || next>=s->history_count) return;
  UITextRevision *r=&s->history[next];
  if(strlen(r->text)>=(size_t)capacity) return; /* Never restore a truncated revision. */
  strcpy(buffer,r->text); s->caret=r->caret; s->selection_anchor=r->anchor;
  s->history_position=next;
}
static int ui_text_low(const UITextInputState *s) { return s->caret<s->selection_anchor?s->caret:s->selection_anchor; }
static int ui_text_high(const UITextInputState *s) { return s->caret>s->selection_anchor?s->caret:s->selection_anchor; }
static bool ui_text_scalar_allowed(uint32_t cp) {
  return cp>=32 && !(cp>=127 && cp<=159) && cp!=0x2028 && cp!=0x2029;
}
static void ui_text_replace(UITextInputState *s,char *buffer,int capacity,const char *text,size_t n) {
  int lo=ui_text_low(s),hi=ui_text_high(s); size_t len=strlen(buffer);
  if(n>(size_t)capacity-1-(len-(size_t)(hi-lo))) return;
  memmove(buffer+lo+n,buffer+hi,len-(size_t)hi+1);
  if(n) memcpy(buffer+lo,text,n);
  s->caret=s->selection_anchor=lo+(int)n;
}
static bool ui_text_copy_selection(UITextInputState *s,const char *buffer,const UIClipboard *clipboard,void *user) {
  int lo=ui_text_low(s),hi=ui_text_high(s);
  if(lo==hi || !clipboard->write) return false;
  size_t n=(size_t)(hi-lo); char *copy=malloc(n+1);
  if(!copy) return false;
  memcpy(copy,buffer+lo,n); copy[n]=0;
  bool ok=clipboard->write(user,copy); free(copy); return ok;
}
static void ui_text_paste(UITextInputState *s,char *buffer,int capacity,const UIClipboard *clipboard,void *user) {
  const char *source=NULL; size_t n=0;
  if(!clipboard->read || !clipboard->read(user,&source,&n)) return;
  /* Bound work on external data. Reject malformed UTF-8 before changing anything. */
  if(source && n<=1048576 && utf8_valid(source,n)) {
    size_t room=(size_t)capacity-1-strlen(buffer)+(size_t)(ui_text_high(s)-ui_text_low(s));
    size_t limit=n<room?n:room; char *copy=malloc(limit+1);
    if(copy) {
      size_t used=0;
      for(size_t p=0;p<n;) {
        uint32_t cp; size_t k=utf8_decode(source+p,n-p,&cp);
        if(ui_text_scalar_allowed(cp)) {
          if(k>limit-used) break;
          memcpy(copy+used,source+p,k); used+=k;
        }
        p+=k;
      }
      /* Empty/unsupported clipboard data must not erase a selection. */
      if(used) ui_text_replace(s,buffer,capacity,copy,used);
      free(copy);
    }
  }
  if(clipboard->release) clipboard->release(user,source);
}
void ui_text_edit_event(UITextInputState *s,char *buffer,int capacity,const InputKeyEvent *e,
                        const UIClipboard *clipboard,void *user) {
  /* AltGr may be reported as Ctrl+Alt: do not turn character composition into
   * select-all/cut/undo shortcuts. Character events remain authoritative. */
  bool ctrl=(e->mods&PLATFORM_MOD_CONTROL)!=0 && !(e->mods&PLATFORM_MOD_ALT);
  bool shift=(e->mods&PLATFORM_MOD_SHIFT)!=0;
  ui_text_history_save(s,buffer); /* Retain selection immediately before the edit. */
  if(e->kind==INPUT_CHARACTER) {
    char encoded[4]; size_t n=utf8_encode(e->codepoint,encoded);
    if(n && ui_text_scalar_allowed(e->codepoint)) ui_text_replace(s,buffer,capacity,encoded,n);
  } else if(e->action==PLATFORM_PRESS || e->action==PLATFORM_REPEAT) {
    int len=(int)strlen(buffer);
    if(ctrl && (e->key==PLATFORM_KEY_Z || e->key==PLATFORM_KEY_Y)) {
      ui_text_history_move(s,buffer,capacity,e->key==PLATFORM_KEY_Y || shift?1:-1); return;
    }
    if(ctrl && e->key==PLATFORM_KEY_A) { s->selection_anchor=0; s->caret=len; }
    else if(ctrl && e->key==PLATFORM_KEY_C) { ui_text_copy_selection(s,buffer,clipboard,user); }
    else if(ctrl && e->key==PLATFORM_KEY_X) {
      if(ui_text_copy_selection(s,buffer,clipboard,user)) ui_text_replace(s,buffer,capacity,"",0);
    } else if(ctrl && e->key==PLATFORM_KEY_V) ui_text_paste(s,buffer,capacity,clipboard,user);
    else if(e->key==PLATFORM_KEY_BACKSPACE || e->key==PLATFORM_KEY_DELETE) {
      if(s->caret==s->selection_anchor) {
        if(e->key==PLATFORM_KEY_BACKSPACE) s->selection_anchor=(int)utf8_prev(buffer,(size_t)s->caret);
        else s->selection_anchor=(int)utf8_next(buffer,(size_t)len,(size_t)s->caret);
      }
      ui_text_replace(s,buffer,capacity,"",0);
    } else if(e->key==PLATFORM_KEY_LEFT || e->key==PLATFORM_KEY_RIGHT ||
              e->key==PLATFORM_KEY_HOME || e->key==PLATFORM_KEY_END) {
      if(e->key==PLATFORM_KEY_HOME) s->caret=0;
      else if(e->key==PLATFORM_KEY_END) s->caret=len;
      else if(!shift && s->caret!=s->selection_anchor)
        s->caret=e->key==PLATFORM_KEY_LEFT?ui_text_low(s):ui_text_high(s);
      else s->caret=(int)(e->key==PLATFORM_KEY_LEFT?utf8_prev(buffer,(size_t)s->caret):utf8_next(buffer,(size_t)len,(size_t)s->caret));
      if(!shift) s->selection_anchor=s->caret;
    }
  }
  ui_text_history_save(s,buffer);
}

/* Escape ripristina il valore all'inizio della modifica. I testi brevi usano
 * lo spazio interno; quelli lunghi una copia posseduta dallo stato del widget.
 * Se l'allocazione fallisce non si deve iniziare una modifica senza snapshot. */
bool ui_text_snapshot(UITextInputState *state, const char *buffer) {
  size_t n=strlen(buffer);
  char *copy=NULL;
  if (n>=sizeof(state->original)) {
    copy=malloc(n+1);
    if (!copy) return false;
    memcpy(copy,buffer,n+1);
  } else utf8_copy(state->original,sizeof(state->original),buffer);
  free(state->original_long); state->original_long=copy;
  state->needs_snapshot=false;
  return true;
}

bool ui_text_edit_prepare(UITextInputState *state,const char *buffer,int capacity,int *out_length) {
  if(!buffer || capacity<=0) return false;
  /* The caller owns the text and may replace it between frames. Reject an
   * unterminated buffer and reconcile the retained cursor before editing. */
  const char *end = memchr(buffer, '\0', (size_t)capacity);
  if (!end)
    return false;
  int length = (int)(end - buffer);
  if (!utf8_valid(buffer, (size_t)length)) return false;
  if (state->caret < 0) state->caret = 0;
  if (state->caret > length) state->caret = length;
  state->caret = (int)utf8_floor(buffer, (size_t)length, (size_t)state->caret);
  if(state->observed && (state->observed_length!=(size_t)length || state->observed_hash!=ui_text_hash(buffer))) {
    ui_text_history_clear(state); /* Caller replaced the document. */
    state->selection_anchor=state->caret;
    if(state->focused && !state->needs_snapshot) state->focused=ui_text_snapshot(state,buffer);
  }
  if(state->selection_anchor<0) state->selection_anchor=0;
  if(state->selection_anchor>length) state->selection_anchor=length;
  state->selection_anchor=(int)utf8_floor(buffer,(size_t)length,(size_t)state->selection_anchor);

  *out_length=length;
  return true;
}

void ui_text_edit_cancel(UITextInputState *state,char *buffer,int capacity) {
  utf8_copy(buffer,(size_t)capacity,state->original_long ? state->original_long : state->original);
  state->caret=state->selection_anchor=(int)strlen(buffer);
  state->focused=false;
  ui_text_history_clear(state);
}
void ui_text_edit_observe(UITextInputState *state,const char *buffer) {
  ui_text_history_save(state,buffer);
  state->observed=true; state->observed_length=strlen(buffer); state->observed_hash=ui_text_hash(buffer);
}
