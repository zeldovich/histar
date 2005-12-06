#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/kbdreg.h>
#include <inc/lib.h>

#define BUFLEN 1024
#define NBUFS 4
static char allbufs[NBUFS][BUFLEN];
static int curbuf = 0;

char*
readline(const char *prompt)
{
	int i, c, echoing;
	char *buf;
	curbuf = (curbuf + 1) % NBUFS;
	buf = &allbufs[curbuf][0];

	if (prompt != NULL)
		cprintf("%s", prompt);

	i = 0;
	echoing = iscons(0);
	while (1) {
		c = getchar();
		if (c < 0) {
			if (c != -E_EOF)
				cprintf("read error: %d\n", c);
			return NULL;
		} else if (c == KEY_UP) {
			buf[i] = 0;
			// delete what is currently viewable
			if (echoing)
				while (i-- > 0)
					putchar('\b');
			i = 0;
			curbuf = (curbuf + NBUFS - 1) % NBUFS;
			buf = &allbufs[curbuf][0];
			while (buf[i]) {
				putchar(buf[i++]);
			}
		} else if (c == KEY_DN) {
			buf[i] = 0;
			// delete what is currently viewable
			if (echoing)
				while (i-- > 0)
					putchar('\b');
			i = 0;
			curbuf = (curbuf + NBUFS + 1) % NBUFS;
			buf = &allbufs[curbuf][0];
			while (buf[i]) {
				putchar(buf[i++]);
			}
		} else if (c >= ' ' && i < BUFLEN-1) {
			if (echoing)
				putchar(c);
			buf[i++] = c;
		}
		else if (c == '\b' && i > 0) {
			if (echoing)
				putchar(c);
			i--;
		}
		else if (c == '\n' || c == '\r') {
			if (echoing)
				putchar(c);
			buf[i] = 0;
			return buf;
		}
	}
}

