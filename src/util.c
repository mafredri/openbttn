#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

int url_decode(char *dest, const char *src, int len) {
  char c;
  const char *end = src + strlen(src);
  char *pDest;

  for (pDest = dest; src <= end && len--; pDest++) {
    c = *src++;
    if (c == '\r' || c == '\n') {
      c = '\0';
    } else if (c == '+') {
      c = ' ';
    } else if (c == '%' && (!isxdigit(*src++) || !isxdigit(*src++) ||
                            !sscanf(src - 2, "%2x", (unsigned int *)&c))) {
      return -1;
    }
    *pDest = c;
  }

  return pDest - dest;
}

uint32_t ip_atoi(const char *ip) {
  unsigned int ipBytes[4];
  if (sscanf(ip, "%u.%u.%u.%u", &ipBytes[3], &ipBytes[2], &ipBytes[1],
             &ipBytes[0]) != 4) {
    return 0;
  }
  return (ipBytes[3] << 24) | (ipBytes[2] << 16) | (ipBytes[1] << 8) |
         (ipBytes[0] << 0);
}

int ip_itoa(char *dest, uint32_t src) {
  return sprintf(dest, "%u.%u.%u.%u", (uint8_t)(src >> 24),
                 (uint8_t)(src >> 16), (uint8_t)(src >> 8),
                 (uint8_t)(src >> 0));
}
