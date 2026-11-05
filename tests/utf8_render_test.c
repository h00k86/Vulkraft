#include "../src/render/text_renderer.c"
#include <assert.h>
float g_virtualWidth=1280,g_virtualHeight=800;
int main(void) {
  Font font={0};
  assert(font_load(&font,"assets/fonts/font_utf8.json",1024,1024));
  assert(font_get(&font,0xe8) && font_get(&font,0x20ac) && font_get(&font,0xfffd));
  const uint32_t cps[]={0x41,0xe8,0x20ac,0x1f642};
  float expected=0;
  for(size_t i=0;i<4;++i) {
    Glyph *g=font_get_fallback(&font,cps[i]);
    expected+=fmaxf(g->plane_right,g->advance)*18;
  }
  assert(fabsf(measure_text_width(&font,"Aè€🙂",18)-expected)<0.0001f);
  g_hudScale=1;
  draw_text_colored(NULL,&font,"Aè€🙂",0,30,18,1,1,1);
  assert(!text_geometry_failed && text_offset==24); // one quad per scalar, including fallback
  assert(measure_text_width(&font,"\xff",18)==measure_text_width(&font,"\xef\xbf\xbd",18));
  free(text_vertices); free(g_hud_batches);
  puts("UTF-8 renderer: scalar quads, measurement, atlas accents/euro, missing and malformed fallback: PASS");
}
