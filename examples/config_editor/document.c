#include "document.h"
#include "platform/platform_fs.h"
#include "text/utf8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static bool fail(char *error,const char *message) { snprintf(error,CONFIG_ERROR_CAP,"%s",message); return false; }
static bool text_valid(const char *s,size_t cap,bool required) {
  const char *end=memchr(s,0,cap); if(!end || (required && end==s)) return false;
  size_t n=(size_t)(end-s); if(!utf8_valid(s,n)) return false;
  for(size_t p=0;p<n;) { uint32_t cp; p+=utf8_decode(s+p,n-p,&cp);
    if(cp<32 || (cp>=127 && cp<=159) || cp==0x2028 || cp==0x2029) return false;
  }
  return true;
}
bool config_document_valid(const ConfigDocument *d) {
  if(!d || d->count<0 || d->count>CONFIG_MAX_ITEMS || !d->next_id) return false;
  for(int i=0;i<d->count;++i) {
    const ConfigItem *v=&d->items[i];
    if(!v->id || v->id>=d->next_id || v->level<0 || v->level>100 ||
       !text_valid(v->name,sizeof(v->name),true)||!text_valid(v->note,sizeof(v->note),false)) return false;
    for(int j=0;j<i;++j) if(v->id==d->items[j].id) return false;
  }
  return true;
}
void config_document_init(ConfigDocument *d) {
  *d=(ConfigDocument){.next_id=1}; config_document_add(d);
}
bool config_document_add(ConfigDocument *d) {
  if(d->count>=CONFIG_MAX_ITEMS || d->next_id==UINT_MAX) return false;
  ConfigItem *v=&d->items[d->count++];
  *v=(ConfigItem){.id=d->next_id++,.enabled=true,.level=50};
  snprintf(v->name,sizeof(v->name),"Profilo %u",v->id); return true;
}
bool config_document_remove(ConfigDocument *d,int index) {
  if(index<0 || index>=d->count) return false;
  memmove(d->items+index,d->items+index+1,(size_t)(d->count-index-1)*sizeof(*d->items));
  --d->count; return true;
}
bool config_document_equal(const ConfigDocument *a,const ConfigDocument *b) {
  if(a->count!=b->count || a->next_id!=b->next_id) return false;
  for(int i=0;i<a->count;++i) {
    const ConfigItem *x=&a->items[i],*y=&b->items[i];
    if(x->id!=y->id || x->enabled!=y->enabled || x->level!=y->level || strcmp(x->name,y->name)||strcmp(x->note,y->note)) return false;
  }
  return true;
}
/* Versioned tab-separated UTF-8. Single-line fields forbid tabs/newlines,
 * so serialization is unambiguous without a bespoke escaping language. */
static bool number(const char *s,unsigned max,unsigned *out) {
  if(!*s) return false;
  unsigned value=0;
  for(;*s;++s) { if(*s<'0'||*s>'9') return false; unsigned digit=(unsigned)(*s-'0');
    if(value>max/10 || (value==max/10 && digit>max%10)) return false;
    value=value*10+digit;
  }
  *out=value; return true;
}
bool config_document_load(ConfigDocument *d,const char *path,char error[CONFIG_ERROR_CAP]) {
  FILE *file=fopen(path,"rb"); if(!file) return fail(error,"Impossibile aprire il file.");
  char *data=malloc(CONFIG_FILE_CAP+1); if(!data) { fclose(file); return fail(error,"Memoria insufficiente."); }
  size_t n=fread(data,1,CONFIG_FILE_CAP+1,file); bool ok=!ferror(file); if(fclose(file)!=0) ok=false;
  if(!ok || n>CONFIG_FILE_CAP || memchr(data,0,n)) { free(data); return fail(error,"File troppo grande o non testuale."); }
  data[n]=0; ConfigDocument candidate={0}; char *line=data; int row=0;
  while(ok && *line) {
    char *end=strchr(line,'\n'); if(!end) { ok=false; break; } *end=0;
    size_t len=strlen(line); if(len && line[len-1]=='\r') line[len-1]=0;
    if(row++==0) {
      const char prefix[]="CONFIG_EDITOR\t1\t";
      ok=!strncmp(line,prefix,sizeof(prefix)-1) && number(line+sizeof(prefix)-1,UINT_MAX,&candidate.next_id) && candidate.next_id;
    } else {
      char *parts[5]; parts[0]=line;
      for(int i=1;i<5;++i) { char *tab=strchr(parts[i-1],'\t'); if(!tab) { ok=false; break; } *tab=0; parts[i]=tab+1; }
      if(ok) {
        unsigned id,on,level;
        ok=candidate.count<CONFIG_MAX_ITEMS && !strchr(parts[4],'\t') && number(parts[0],UINT_MAX,&id) &&
           number(parts[1],1,&on) && number(parts[2],100,&level) && strlen(parts[3])<CONFIG_NAME_CAP && strlen(parts[4])<CONFIG_NOTE_CAP;
        if(ok) { ConfigItem *v=&candidate.items[candidate.count++]; *v=(ConfigItem){.id=id,.enabled=on!=0,.level=(int)level}; strcpy(v->name,parts[3]); strcpy(v->note,parts[4]); }
      }
    }
    line=end+1;
  }
  free(data);
  if(!ok || !row || !config_document_valid(&candidate)) return fail(error,"Formato non valido: atteso un documento .cfgapp versione 1.");
  *d=candidate; error[0]=0; return true;
}
bool config_document_save(const ConfigDocument *d,const char *path,char error[CONFIG_ERROR_CAP]) {
  if(!config_document_valid(d)) return fail(error,"Controlla i nomi: obbligatori, UTF-8 e su una sola riga.");
  char *data=malloc(CONFIG_FILE_CAP); if(!data) return fail(error,"Memoria insufficiente.");
  int n=snprintf(data,CONFIG_FILE_CAP,"CONFIG_EDITOR\t1\t%u\n",d->next_id); size_t used=(size_t)n; bool ok=n>0;
  for(int i=0;ok && i<d->count;++i) {
    const ConfigItem *v=&d->items[i]; n=snprintf(data+used,CONFIG_FILE_CAP-used,"%u\t%d\t%d\t%s\t%s\n",v->id,v->enabled?1:0,v->level,v->name,v->note);
    ok=n>=0 && (size_t)n<CONFIG_FILE_CAP-used; if(ok) used+=(size_t)n;
  }
  if(ok) ok=platform_write_file_atomic(path,data,used);
  free(data); if(!ok) return fail(error,"Salvataggio fallito. Il documento resta modificato; il file precedente e' conservato.");
  error[0]=0; return true;
}
