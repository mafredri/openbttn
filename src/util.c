#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "util.h"

int url_decode(char *dest, const char *src) {
  char c;
  const char *end = src + strlen(src);
  char *dPtr;

  for (dPtr = dest; src <= end; dPtr++) {
    c = *src++;
    if (c == '\r' || c == '\n') {
      c = '\0';
    } else if (c == '+') {
      c = ' ';
    } else if (c == '%' && (!isxdigit(*src++) || !isxdigit(*src++) ||
                            !sscanf(src - 2, "%2x", (unsigned int *)&c))) {
      return -1;
    }
    *dPtr = c;
  }

  return dPtr - dest;
}
