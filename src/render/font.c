#include "render/font.h"
#include <stdbool.h>
#include <math.h>
#include <errno.h>
#include <float.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Ricerca binaria nella tabella ordinata per punto di codice. MAX_GLYPHS
 * limita il numero di glifi memorizzati, non il valore Unicode ricercabile. */
Glyph *font_get(Font *font, uint32_t unicode) {
  if (!font || font->glyph_count > MAX_GLYPHS) return NULL;
  uint32_t lo=0, hi=font->glyph_count;
  while (lo<hi) {
    uint32_t mid=lo+(hi-lo)/2;
    if (font->glyphs[mid].unicode<unicode) lo=mid+1; else hi=mid;
  }
  return lo<font->glyph_count && font->glyphs[lo].unicode==unicode ? &font->glyphs[lo] : NULL;
}
Glyph *font_get_fallback(Font *font, uint32_t unicode) {
  Glyph *g=font_get(font,unicode);
  if (!g) g=font_get(font,0xfffd);
  if (!g) g=font_get(font,'?');
  return g;
}
static int compare_glyphs(const void *a, const void *b) {
  uint32_t x=((const Glyph *)a)->unicode, y=((const Glyph *)b)->unicode;
  return (x>y)-(x<y);
}

int font_load(Font *font, const char *json_file, uint32_t atlas_width,
              uint32_t atlas_height) {
  return font_load_at(font, json_file, atlas_width, atlas_height, 0.0f);
}


/* Small bounded JSON reader. Tokens keep subtree boundaries, so fields are
 * selected by object membership rather than by substring searches. */
typedef struct { const char *start, *end; int next; char type; } FontToken;
typedef struct {
  const char *p, *end;
  FontToken *tokens;
  int count, capacity;
} FontJson;

static void font_json_space(FontJson *j) {
  while (j->p < j->end && strchr(" \t\r\n", *j->p)) ++j->p;
}
static bool font_json_string(FontJson *j) {
  if (j->p == j->end || *j->p++ != '"') return false;
  while (j->p < j->end) {
    unsigned char c = (unsigned char)*j->p++;
    if (c == '"') return true;
    if (c < 32) return false;
    if (c == '\\') {
      if (j->p == j->end) return false;
      c = (unsigned char)*j->p++;
      if (c == 'u') {
        for (int k = 0; k < 4; ++k)
          if (j->p == j->end || !isxdigit((unsigned char)*j->p++)) return false;
      } else if (!strchr("\"\\/bfnrt", c)) return false;
    }
  }
  return false;
}
static int font_json_value(FontJson *j, int depth) {
  font_json_space(j);
  if (depth > 32 || j->p == j->end || j->count >= 65536) return -1;
  if (j->count == j->capacity) {
    int capacity = j->capacity ? j->capacity * 2 : 256;
    FontToken *tokens = realloc(j->tokens, (size_t)capacity * sizeof(*tokens));
    if (!tokens) return -1;
    j->tokens = tokens; j->capacity = capacity;
  }
  int index = j->count++;
  const char *start = j->p;
  char type = *j->p;
  if (type == '{' || type == '[') {
    char close = type == '{' ? '}' : ']';
    ++j->p; font_json_space(j);
    if (j->p < j->end && *j->p != close) {
      for (;;) {
        if (type == '{') {
          if (j->p == j->end || *j->p != '"' || font_json_value(j, depth + 1) < 0) return -1;
          font_json_space(j);
          if (j->p == j->end || *j->p++ != ':') return -1;
        }
        if (font_json_value(j, depth + 1) < 0) return -1;
        font_json_space(j);
        if (j->p == j->end) return -1;
        if (*j->p != ',') break;
        ++j->p; font_json_space(j);
      }
    }
    if (j->p == j->end || *j->p++ != close) return -1;
  } else if (type == '"') {
    if (!font_json_string(j)) return -1;
  } else if (type == '-' || (type >= '0' && type <= '9')) {
    type = 'n';
    if (*j->p == '-') ++j->p;
    if (j->p == j->end) return -1;
    if (*j->p == '0') ++j->p;
    else {
      if (*j->p < '1' || *j->p > '9') return -1;
      do { ++j->p; } while (j->p < j->end && isdigit((unsigned char)*j->p));
    }
    if (j->p < j->end && *j->p == '.') {
      ++j->p;
      if (j->p == j->end || !isdigit((unsigned char)*j->p)) return -1;
      do { ++j->p; } while (j->p < j->end && isdigit((unsigned char)*j->p));
    }
    if (j->p < j->end && (*j->p == 'e' || *j->p == 'E')) {
      ++j->p;
      if (j->p < j->end && (*j->p == '+' || *j->p == '-')) ++j->p;
      if (j->p == j->end || !isdigit((unsigned char)*j->p)) return -1;
      do { ++j->p; } while (j->p < j->end && isdigit((unsigned char)*j->p));
    }
  } else {
    const char *literal = type == 't' ? "true" : type == 'f' ? "false" : type == 'n' ? "null" : NULL;
    if (!literal || (size_t)(j->end - j->p) < strlen(literal) ||
        memcmp(j->p, literal, strlen(literal))) return -1;
    j->p += strlen(literal); type = 'l';
  }
  j->tokens[index] = (FontToken){start, j->p, j->count, type};
  return index;
}
static bool font_json_key(const FontToken *token, const char *key) {
  const char *p = token->start + 1, *end = token->end - 1;
  while (p < end && *key) {
    unsigned c = (unsigned char)*p++;
    if (c == '\\') {
      c = (unsigned char)*p++;
      if (c == 'u') {
        c = 0;
        for (int i = 0; i < 4; ++i) {
          unsigned char h = (unsigned char)*p++;
          c = c * 16 + (isdigit(h) ? (unsigned)(h - '0') : (unsigned)tolower(h) - 'a' + 10);
        }
      }
    }
    if (c != (unsigned char)*key++) return false;
  }
  return p == end && !*key;
}
static int font_json_field(const FontJson *j, int object, const char *key) {
  if (object < 0 || j->tokens[object].type != '{') return -2;
  int found = -1;
  for (int i = object + 1; i < j->tokens[object].next;) {
    int value = i + 1;
    if (font_json_key(&j->tokens[i], key)) {
      if (found != -1) return -2; /* Duplicate required field. */
      found = value;
    }
    i = j->tokens[value].next;
  }
  return found;
}
static bool font_json_number(const FontJson *j, int token, float *out) {
  if (token < 0 || j->tokens[token].type != 'n') return false;
  char *end; errno = 0;
  float value = strtof(j->tokens[token].start, &end);
  if (end != j->tokens[token].end || errno == ERANGE || !isfinite(value)) return false;
  *out = value; return true;
}
static bool font_json_bounds(const FontJson *j, int object, float *l, float *b, float *r, float *t) {
  return font_json_number(j, font_json_field(j, object, "left"), l) &&
         font_json_number(j, font_json_field(j, object, "bottom"), b) &&
         font_json_number(j, font_json_field(j, object, "right"), r) &&
         font_json_number(j, font_json_field(j, object, "top"), t) && *r >= *l && *t >= *b;
}
/* Caricamento transazionale: costruisce un Font temporaneo, valida limiti,
 * coordinate e duplicati, poi sostituisce il font del chiamante solo al successo.
 * offset_x permette di collocare le coordinate in un atlante piu ampio. */
int font_load_at(Font *font, const char *json_file, uint32_t atlas_width,
                 uint32_t atlas_height, float offset_x) {
  if (!font || !json_file || !atlas_width || !atlas_height || !isfinite(offset_x) || offset_x < 0) return 0;
  FILE *f = fopen(json_file, "rb");
  if (!f) return 0;
  long size = -1;
  if (fseek(f, 0, SEEK_END) == 0) size = ftell(f);
  if (size <= 0 || size > 4 * 1024 * 1024 || fseek(f, 0, SEEK_SET) != 0) {
    fclose(f); return 0;
  }
  char *data = malloc((size_t)size + 1);
  if (!data) { fclose(f); return 0; }
  bool read_ok = fread(data, 1, (size_t)size, f) == (size_t)size && !ferror(f);
  fclose(f); data[size] = 0;
  FontJson j = {.p = data, .end = data + size};
  Font parsed = {.atlas_width = atlas_width, .atlas_height = atlas_height, .ascender = 0.93f};
  bool ok = false;
  if (!read_ok || memchr(data, 0, (size_t)size) || font_json_value(&j, 0) != 0) goto done;
  font_json_space(&j);
  if (j.p != j.end || j.tokens[0].type != '{') goto done;
  int metrics = font_json_field(&j, 0, "metrics");
  if (metrics == -2) goto done;
  if (metrics >= 0 && !font_json_number(&j, font_json_field(&j, metrics, "ascender"), &parsed.ascender)) goto done;
  int glyphs = font_json_field(&j, 0, "glyphs");
  if (glyphs < 0 || j.tokens[glyphs].type != '[' || j.tokens[glyphs].next == glyphs + 1) goto done;
  for (int i = glyphs + 1; i < j.tokens[glyphs].next; i = j.tokens[i].next) {
    Glyph g = {0}; float code;
    if (!font_json_number(&j, font_json_field(&j, i, "unicode"), &code) ||
        code < 1 || code > 0x10ffff || floorf(code) != code ||
        (code >= 0xd800 && code <= 0xdfff) ||
        !font_json_number(&j, font_json_field(&j, i, "advance"), &g.advance)) goto done;
    g.unicode = (uint32_t)code;
    int plane = font_json_field(&j, i, "planeBounds"), atlas = font_json_field(&j, i, "atlasBounds");
    if (plane == -2 || atlas == -2 || (plane < 0) != (atlas < 0)) goto done;
    if (plane >= 0) {
      float l, b, r, t;
      if (!font_json_bounds(&j, plane, &g.plane_left, &g.plane_bottom, &g.plane_right, &g.plane_top) ||
          !font_json_bounds(&j, atlas, &l, &b, &r, &t) || l < 0 || b < 0 ||
          r + offset_x > atlas_width || t > atlas_height) goto done;
      g.u0 = (l + offset_x) / atlas_width; g.u1 = (r + offset_x) / atlas_width;
      g.v0 = 1 - t / atlas_height; g.v1 = 1 - b / atlas_height;
    }
    if (parsed.glyph_count == MAX_GLYPHS) goto done;
    parsed.glyphs[parsed.glyph_count++] = g;
  }
  qsort(parsed.glyphs,parsed.glyph_count,sizeof(Glyph),compare_glyphs);
  for (uint32_t i=1;i<parsed.glyph_count;++i)
    if (parsed.glyphs[i-1].unicode==parsed.glyphs[i].unicode) goto done;
  *font = parsed; ok = true;
done:
  free(j.tokens); free(data);
  return ok;
}
