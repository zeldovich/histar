// Stripped-down primitive printf-style formatting routines,
// used in common by printf, sprintf, fprintf, etc.
// This code is also used by both the kernel and user programs.

#include <inc/types.h>
#include <inc/error.h>

#ifdef JOS_KERNEL
#include <kern/lib.h>
#else /* !JOS_KERNEL */
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/stdarg.h>
#endif /* !JOS_KERNEL */

/*
 * Space or zero padding and a field width are supported for the numeric
 * formats only. 
 */

static const char *const error_string[E_MAXERROR + 1] = {
    [0]		  = "no error",
    [E_UNSPEC]	  = "unspecified error",
    [E_INVAL]	  = "invalid parameter",
    [E_NO_MEM]	  = "out of memory",
    [E_RESTART]	  = "restart system call",
    [E_NOT_FOUND] = "object not found",
    [E_LABEL]	  = "label check failure",
    [E_BUSY]	  = "device busy",
    [E_NO_SPACE]  = "not enough space in buffer",
    [E_AGAIN]	  = "try again",
    [E_IO]	  = "disk IO error",
    [E_RANGE]	  = "value out of range",
    [E_EOF]	  = "unexpected end-of-file",
    [E_MAX_OPEN]  = "out of file descriptors",
    [E_BAD_OP]    = "operation not supported",
    [E_EXISTS]	  = "already exists",
    [E_MAXERROR]  = "error code out of range",
};

const char *
e2s(int err) {
    if (err < 0)
	err = -err;
    if (err > E_MAXERROR)
	err = E_MAXERROR;
    const char *s = error_string[err];
    if (s == 0)
	s = "missing error definition in error_string[] table";
    return s;
}

/*
 * Print a number (base <= 16) in reverse order,
 * using specified putch function and associated pointer putdat.
 */
static void
printnum (void (*putch) (int, void *), void *putdat,
	  unsigned long long num, unsigned base, int width, int padc)
{
  // recursion a bad idea here
  char buf[68], *x;

  for (x = buf; num; num /= base)
    *x++ = "0123456789abcdef"[num % base];
  if (x == buf)
    *x++ = '0';

  if (padc != '-')
    for (; width > x - buf; width--)
      putch (padc, putdat);

  for (; x > buf; width--)
    putch (*--x, putdat);

  if (padc == '-')
    for (; width > 0; width--)
      putch (' ', putdat);
}

// Get an unsigned int of various possible sizes from a varargs list,
// depending on the lflag parameter.
static unsigned long long
getuint (va_list ap, int lflag)
{
  if (lflag >= 2)
    return va_arg (ap, unsigned long long);
  else if (lflag)
    return va_arg (ap, unsigned long);
  else
    return va_arg (ap, unsigned int);
}

// Same as getuint but signed - can't use getuint
// because of sign extension
static long long
getint (va_list ap, int lflag)
{
  if (lflag >= 2)
    return va_arg (ap, long long);
  else if (lflag)
    return va_arg (ap, long);
  else
    return va_arg (ap, int);
}


void
vprintfmt (void (*putch) (int, void *), void *putdat, const char *fmt,
	   va_list ap)
{
  register const char *p;
  register int ch;
  unsigned long long num;
  int base, lflag, width, precision, altflag;
  char padc;

  while (1) {
    while ((ch = *(unsigned char *) fmt++) != '%') {
      if (ch == '\0')
	return;
      putch (ch, putdat);
    }

    // Process a %-escape sequence
    padc = ' ';
    width = -1;
    precision = -1;
    lflag = 0;
    altflag = 0;
  reswitch:
    switch (ch = *(unsigned char *) fmt++) {

      // flag to pad on the right
    case '-':
      padc = '-';
      goto reswitch;

      // flag to pad with 0's instead of spaces
    case '0':
      padc = '0';
      goto reswitch;

      // width field
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      for (precision = 0;; ++fmt) {
	precision = precision * 10 + ch - '0';
	ch = *fmt;
	if (ch < '0' || ch > '9')
	  break;
      }
      goto process_precision;

    case '*':
      precision = va_arg (ap, int);
      goto process_precision;

    case '.':
      if (width < 0)
	width = 0;
      goto reswitch;

    case '#':
      altflag = 1;
      goto reswitch;

    process_precision:
      if (width < 0)
	width = precision, precision = -1;
      goto reswitch;

      // long flag (doubled for long long)
    case 'l':
      lflag++;
      goto reswitch;

      // character
    case 'c':
      putch (va_arg (ap, int), putdat);
      break;

      // string
    case 's':
      if ((p = va_arg (ap, char *)) == NULL)
	p = "(null)";
      if (width > 0 && padc != '-')
	for (width -= strlen (p); width > 0; width--)
	  putch (padc, putdat);
      for (; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0);
	   width--)
	if (altflag && (ch < ' ' || ch > '~'))
	  putch ('?', putdat);
	else
	  putch (ch, putdat);
      for (; width > 0; width--)
	putch (' ', putdat);
      break;

      // binary
    case 'b':
      num = getint (ap, lflag);
      base = 2;
      goto number;

      // (signed) decimal
    case 'd':
      num = getint (ap, lflag);
      if ((long long) num < 0) {
	putch ('-', putdat);
	num = -(long long) num;
      }
      base = 10;
      goto number;

      // unsigned decimal
    case 'u':
      num = getuint (ap, lflag);
      base = 10;
      goto number;

      // (unsigned) octal
    case 'o':
      num = getuint (ap, lflag);
      base = 8;
      goto number;

      // pointer
    case 'p':
      putch ('0', putdat);
      putch ('x', putdat);
      num = (unsigned long long)
	(uintptr_t) va_arg (ap, void *);
      base = 16;
      goto number;

      // (unsigned) hexadecimal
    case 'x':
      num = getuint (ap, lflag);
      base = 16;
    number:
      printnum (putch, putdat, num, base, MAX (width, 0), padc);
      break;

      // unrecognized escape sequence - just print it literally
    default:
      putch ('%', putdat);
      while (lflag-- > 0)
	putch ('l', putdat);
      /* FALLTHROUGH */

      // escaped '%' character
    case '%':
      putch (ch, putdat);
    }
  }
}

void
printfmt (void (*putch) (int, void *), void *putdat, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vprintfmt (putch, putdat, fmt, ap);
  va_end (ap);
}

struct sprintbuf
{
  char *buf;
  char *ebuf;
  int cnt;
};

static void
sprintputch (int ch, struct sprintbuf *b)
{
  b->cnt++;
  if (b->buf < b->ebuf)
    *b->buf++ = ch;
}

int
vsnprintf (char *buf, size_t n, const char *fmt, va_list ap)
{
  struct sprintbuf b = { buf, buf + n - 1, 0 };

  if (buf == NULL || n < 1)
    return -E_INVAL;

  // print the string to the buffer
  vprintfmt ((void *) sprintputch, &b, fmt, ap);

  // null terminate the buffer
  *b.buf = '\0';

  return b.cnt;
}

int
snprintf (char *buf, size_t n, const char *fmt, ...)
{
  va_list ap;
  int rc;

  va_start (ap, fmt);
  rc = vsnprintf (buf, n, fmt, ap);
  va_end (ap);

  return rc;
}

int
sprintf (char *buf, const char *fmt, ...)
{
  va_list ap;
  int cnt;

  va_start (ap, fmt);
  cnt = vsnprintf (buf, 100000, fmt, ap);
  va_end (ap);

  return cnt;
}
