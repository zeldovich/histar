// Implementation of cprintf console output for user environments,
// based on printfmt() and the sys_cons_puts() system call.
//
// cprintf is a debugging statement, not a generic output statement.
// It is very important that it always go to the console, especially when 
// debugging file descriptor code!

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/fd.h>

// Collect up to 256 characters into a buffer
// and perform ONE system call to print all of them,
// in order to make the lines output to the console atomic
// and prevent interrupts from causing context switches
// in the middle of a console output line and such.
struct printbuf {
	int idx;	// current buffer index
	int cnt;	// total bytes printed so far
	char buf[256];

	void (*flush) (struct printbuf *);
	int fd;
};

static void
cons_flush(struct printbuf *b)
{
	sys_cons_puts(b->buf, b->idx);
	b->idx = 0;
}

static void
fd_flush(struct printbuf *b)
{
	write(b->fd, b->buf, b->idx);
	b->idx = 0;
}

static void
flush(struct printbuf *b)
{
	b->flush(b);
}

static void
putch(int ch, struct printbuf *b)
{
	b->buf[b->idx++] = ch;
	if (b->idx == 256)
		flush(b);
	b->cnt++;
}

int
vcprintf(const char *fmt, va_list ap)
{
	struct printbuf b;

	b.idx = 0;
	b.cnt = 0;
	b.flush = &cons_flush;
	vprintfmt((void*)putch, &b, fmt, ap);
	flush(&b);

	return b.cnt;
}

int
cprintf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vcprintf(fmt, ap);
	va_end(ap);

	return cnt;
}

int
vprintf(const char *fmt, va_list ap)
{
	struct printbuf b;

	b.idx = 0;
	b.cnt = 0;
	b.fd = 1;
	b.flush = &fd_flush;
	vprintfmt((void*)putch, &b, fmt, ap);
	flush(&b);

	return b.cnt;
}

int
printf(const char *fmt, ...)
{
	va_list ap;
	int cnt;

	va_start(ap, fmt);
	cnt = vprintf(fmt, ap);
	va_end(ap);

	return cnt;
}
