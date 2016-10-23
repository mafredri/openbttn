#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

int url_decode(char *dest, const char *src, int len);
uint32_t ip_atoi(const char *ip);
int ip_itoa(char *dest, uint32_t src);

#endif /* UTIL_H */
