#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>


#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

char *itoa(int num, char *str, int radix){
  char string[] = "0123456789abcdefghijklmnopqrstuvwxyz";

  char* ptr = str;
  int i;
  int j;
  if(!num){
    str="0";
    return str;
  }

  while (num){
    *ptr++ = string[num % radix];
    num /= radix;

    if (num < radix){
      *ptr++ = string[num];
      *ptr = '\0';
      break;
    }
  }

  j = ptr - str - 1;

  for (i = 0; i < (ptr - str) / 2; i++){
    int temp = str[i];
    str[i] = str[j];
    str[j--] = temp;
  }

  return str;
}


int printf(const char *fmt, ...) {
  //panic("Not implemented");
  va_list arg;
  int done = 0;

  va_start (arg, fmt);

  while( *fmt != '\0'){
    if( *fmt == '%'){
      if( *(fmt+1) == 'c' ){
        char c = (char)va_arg(arg, int);
        putch(c);
      } 
      else if( *(fmt+1) == 'd' || *(fmt+1) == 'i'){
        char store[20];
        int i = va_arg(arg, int);
        char* str = store;
        itoa(i, store, 10);
        while( *str != '\0') putch(*str++);
      } 
      else if( *(fmt+1) == 'o'){
        char store[20];
        int i = va_arg(arg, int);
        char* str = store;
        itoa(i, store, 8);
        while( *str != '\0') putch(*str++);
      } 
      else if( *(fmt+1) == 'x'||*(fmt+1) == 'p'){
        char store[20];
        int i = va_arg(arg, int);
        char* str = store;
        itoa(i, store, 16);
        while( *str != '\0') putch(*str++);
      } 
      else if( *(fmt+1) == 's' ){
        char* str = va_arg(arg, char*);
        while( *str != '\0') putch(*str++);
      }

      fmt += 2;
    } 
    else {
      putch(*fmt++);
    }
  }

  va_end (arg);

  return done;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  panic("Not implemented");
}

int sprintf(char *out, const char *fmt, ...) {
  va_list args;
    va_start(args, fmt);

    const char *p = fmt;
    char *buf_ptr = out;

    while (*p != '\0') {
        if (*p == '%') {
            p++;
            if (*p == 'd') {
                int i = va_arg(args, int);
                buf_ptr += sprintf(buf_ptr, "%d", i);
            } else if (*p == 's') {
                char *s = va_arg(args, char*);
                buf_ptr += sprintf(buf_ptr, "%s", s);
            } else {
                // 如果不是支持的格式说明符，则直接复制字符
                *buf_ptr++ = '%';
                *buf_ptr++ = *p;
            }
        } else {
            *buf_ptr++ = *p;
        }
        p++;
    }

    *buf_ptr = '\0';  // 确保结果字符串以空字符结尾

    va_end(args);
    return buf_ptr - out;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list args;
    va_start(args, fmt);

    const char *p = fmt;
    char *buf_ptr = out;
    size_t remaining = n;

    while (*p != '\0' && remaining > 1) {  // 保留一个字符位置用于终止符
        if (*p == '%') {
            p++;
            if (*p == 'd') {
                int i = va_arg(args, int);
                int len = snprintf(buf_ptr, remaining, "%d", i);
                if (len >= remaining) len = remaining - 1;
                buf_ptr += len;
                remaining -= len;
            } else if (*p == 's') {
                char *s = va_arg(args, char*);
                int len = snprintf(buf_ptr, remaining, "%s", s);
                if (len >= remaining) len = remaining - 1;
                buf_ptr += len;
                remaining -= len;
            } else {
                // 如果不是支持的格式说明符，则直接复制字符
                if (remaining > 2) { // 确保有足够的空间
                    *buf_ptr++ = '%';
                    *buf_ptr++ = *p;
                    remaining -= 2;
                } else {
                    break; // 没有足够的空间来放置更多字符
                }
            }
        } else {
            *buf_ptr++ = *p;
            remaining--;
        }
        p++;
    }

    if (remaining > 0) {
        *buf_ptr = '\0';  // 确保结果字符串以空字符结尾
    }

    va_end(args);
    return buf_ptr - out;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
