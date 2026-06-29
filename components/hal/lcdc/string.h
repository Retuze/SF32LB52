#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int strcmp(const char *a, const char *b);
char *strdup(const char *s);
#ifdef __cplusplus
}
#endif
