// Simple implementation of cprintf console output for the kernel,
// based on printfmt() and the kernel console's putchar().

#include <kern/lib.h>
#include <kern/console.h>
#include <inc/types.h>

static void
putch (int ch, void *cnt)
{
  cons_putc(ch);
  (*((int *) cnt))++;
}

int
vcprintf (const char *fmt, va_list ap)
{
  int cnt = 0;

  vprintfmt (putch, &cnt, fmt, ap);
  return cnt;
}

int
cprintf (const char *fmt, ...)
{
  va_list ap;
  int cnt;

  va_start (ap, fmt);
  cnt = vcprintf (fmt, ap);
  va_end (ap);

  return cnt;
}
