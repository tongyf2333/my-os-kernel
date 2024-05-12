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
  panic("Not implemented");
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
