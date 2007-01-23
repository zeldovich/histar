#include <kern/lib.h>

int
cprintf (const char *fmt, ...)
{
  va_list ap;
  int cnt;

  va_start (ap, fmt);
  cnt = vprintf (fmt, ap);
  va_end (ap);

  return cnt;
}
