#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Strict Unicode scalar decoder. Invalid input consumes one byte and produces
 * U+FFFD, so rendering always advances; never reads beyond n bytes. */
static inline size_t utf8_decode(const char *s, size_t n, uint32_t *cp) {
  *cp = 0xfffd;
  if (!n) return 0;
  const unsigned char *p = (const unsigned char *)s;
  if (p[0] < 0x80) { *cp=p[0]; return 1; }
  size_t k = p[0]>=0xc2 && p[0]<=0xdf ? 2 :
             p[0]>=0xe0 && p[0]<=0xef ? 3 : p[0]>=0xf0 && p[0]<=0xf4 ? 4 : 0;
  if (!k || n<k) return 1;
  uint32_t value=p[0] & (0x7f >> k);
  for (size_t i=1;i<k;++i) {
    if ((p[i]&0xc0)!=0x80) return 1;
    value=(value<<6)|(p[i]&0x3f);
  }
  if ((k==2 && value<0x80) || (k==3 && value<0x800) ||
      (k==4 && value<0x10000) || value>0x10ffff ||
      (value>=0xd800 && value<=0xdfff)) return 1;
  *cp=value; return k;
}
/* Scrive da 1 a 4 byte, senza terminatore NUL. out deve avere almeno 4 byte.
 * Restituisce 0 per valori che non sono scalari Unicode validi. */
static inline size_t utf8_encode(uint32_t cp, char out[4]) {
  if (cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
  if (cp<0x80) { out[0]=(char)cp; return 1; }
  size_t n=cp<0x800 ? 2 : cp<0x10000 ? 3 : 4;
  uint32_t value=cp;
  for (size_t i=n-1;i>0;--i) { out[i]=(char)(0x80|(value&0x3f)); value>>=6; }
  out[0]=(char)((n==2 ? 0xc0 : n==3 ? 0xe0 : 0xf0)|value);
  return n;
}
/* Controlla esattamente n byte; il terminatore della stringa e responsabilita
 * del chiamante. Una vera sequenza U+FFFD e valida, un byte invalido non lo e. */
static inline bool utf8_valid(const char *s, size_t n) {
  for (size_t i=0;i<n;) {
    uint32_t cp; size_t k=utf8_decode(s+i,n-i,&cp);
    if (k==1 && (unsigned char)s[i]>=0x80) return false;
    i+=k;
  }
  return true;
}
/* Byte offsets at scalar boundaries. Valid UTF-8 is required by editor callers. */
static inline size_t utf8_floor(const char *s, size_t n, size_t offset) {
  if (offset>=n) return n;
  while (offset && ((unsigned char)s[offset]&0xc0)==0x80) --offset;
  return offset;
}
static inline size_t utf8_prev(const char *s, size_t offset) {
  return offset ? utf8_floor(s,offset,offset-1) : 0;
}
static inline size_t utf8_next(const char *s, size_t n, size_t offset) {
  uint32_t cp;
  return offset<n ? offset+utf8_decode(s+offset,n-offset,&cp) : n;
}
/* Copia una stringa UTF-8 valida e terminata, senza tagliare un carattere.
 * capacity include il NUL; supporta sovrapposizione fra sorgente e destinazione.
 * Non converte ne ripara sorgenti malformate. */
static inline void utf8_copy(char *dst, size_t capacity, const char *src) {
  if (!capacity) return;
  size_t n=strlen(src), take=n<capacity ? n : utf8_floor(src,n,capacity-1);
  memmove(dst,src,take); dst[take]=0;
}
