#include <stdio.h>
#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <stddef.h>

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
		printf("%s", prompt);

	i = 0;
	echoing = iscons(0);
	while (1) {
		c = getchar();
		if (c < 0) {
			if (c != -E_EOF)
				printf("read error: %s\n", e2s(c));
			return NULL;
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
			if (echoing) {
				putchar('\r');
				putchar('\n');
			}
			buf[i] = 0;
			fflush(stdout);
			return buf;
		}
	}
}
