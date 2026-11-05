#define _POSIX_C_SOURCE 200809L
#include "render/font.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void fixture(const char *path, const char *json) {
  FILE *f = fopen(path, "wb"); assert(f);
  assert(fwrite(json, 1, strlen(json), f) == strlen(json)); assert(!fclose(f));
}
int main(void) {
  Font font = {0};
  assert(font_load(&font, "assets/fonts/font_utf8.json", 1024, 1024));
  assert(font_get(&font, 'A'));
  char path[] = "/tmp/graphics-font-XXXXXX";
  int fd = mkstemp(path); assert(fd >= 0); close(fd);
  fixture(path, "{\"glyphs\":[{\"advance\":0.5,\"unicode\":32},{\"atlasBounds\":{\"top\":20,\"right\":10,\"bottom\":0,\"left\":0},\"advance\":1,\"planeBounds\":{\"top\":1,\"bottom\":0,\"right\":1,\"left\":0},\"unicode\":65}],\"metrics\":{\"ascender\":0.8}}");
  assert(font_load(&font, path, 512, 512));
  assert(font_get(&font, 32)->plane_right == 0);
  assert(font_get(&font, 65)->u1 == 10.0f/512);
  Font saved = font;
  const char *bad[] = {"", "{", "{\"glyphs\":[]}",
    "{\"glyphs\":[{\"unicode\":65,\"advance\":1,\"planeBounds\":{}}]}",
    "{\"glyphs\":[{\"unicode\":65,\"advance\":1,\"advance\":2}]}",
    "{\"glyphs\":[{\"unicode\":65,\"advance\":1e999}]}",
    "{\"glyphs\":[{\"unicode\":65,\"advance\":1},]}",
    "{\"glyphs\":[{\"unicode\":65,\"advance\":1}]}trailing"};
  for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
    fixture(path, bad[i]); assert(!font_load(&font, path, 512, 512));
    assert(!memcmp(&saved, &font, sizeof(font)));
  }
  fixture(path,"{\"glyphs\":[{\"unicode\":128578,\"advance\":1},{\"unicode\":8364,\"advance\":1},{\"unicode\":65,\"advance\":1}]}");
  assert(font_load(&font,path,512,512));
  assert(font.glyph_count==3 && font_get(&font,128578) && font_get(&font,8364));
  assert(font.glyphs[0].unicode==65);
  saved=font;
  fixture(path,"{\"glyphs\":[{\"unicode\":128578,\"advance\":1},{\"unicode\":128578,\"advance\":1}]}");
  assert(!font_load(&font,path,512,512) && !memcmp(&font,&saved,sizeof(font)));
  FILE *many=fopen(path,"wb"); assert(many); fputs("{\"glyphs\":[",many);
  for(unsigned i=0;i<=MAX_GLYPHS;++i)
    fprintf(many,"%s{\"unicode\":%u,\"advance\":1}",i ? "," : "",i+1);
  fputs("]}",many); fclose(many);
  assert(!font_load(&font,path,512,512) && !memcmp(&font,&saved,sizeof(font)));
  unlink(path);
  assert(!font_load(&font, path, 512, 512));
  puts("Font parser valid/reordered/malformed/atomic load: PASS");
}
