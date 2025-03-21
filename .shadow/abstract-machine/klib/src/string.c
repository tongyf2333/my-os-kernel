#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
   const char *ss = s;
    while (*s) {
        ss++;
    }
    return ss - s;
}

char *strcpy(char *dst, const char *src) {
  if (dst == NULL || src == NULL) return NULL;
  char *dest_ptr = dst;
  while ((*dest_ptr++ = *src++) != '\0') {}
  return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
  panic("Not implemented");
}

char *strcat(char *dst, const char *src) {
  panic("Not implemented");
}

int strcmp(const char *s1, const char *s2) {
  for(;*s1 == *s2; s1++, s2++){	
		if (*s1 == '\0')
		return 0;
	}
	if(*(unsigned char*)s1 > *(unsigned char*)s2){
		return 1;
	}
	else if(*(unsigned char*)s1 < *(unsigned char*)s2){
		return -1;
	}
  return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  panic("Not implemented");
}

void *memset(void *s, int c, size_t n) {
  const unsigned char uc = c;
  unsigned char *su;
  for(su = s;0 < n;++su,--n)
    *su = uc;
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  panic("Not implemented");
}

void *memcpy(void *out, const void *in, size_t n) {
  //panic("Not implemented");
  char *d = (char *)out;
  const char *s = (const char *)in;
  for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
  }

  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  panic("Not implemented");
}

#endif
