#include <stdio.h>
#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <inc/pty.h>
#include <stddef.h>
#include <string.h>

#define BUFLEN 1024
#define NBUFS 4
static char allbufs[NBUFS][BUFLEN];
static int curbuf = 0;

char*
readline(const char *prompt, int echoing)
{
	struct termios term;
	ioctl(0, TCGETS, &term);

	struct termios nterm;
	memcpy(&nterm, &term, sizeof(term));

	if (echoing) {
		nterm.c_oflag |= ONLCR;
		nterm.c_lflag |= ECHO;
	} else {
		nterm.c_lflag &= ~ECHO;
	}
	ioctl(0, TCSETS, &nterm);

	int i, c;
	char *buf;
	curbuf = (curbuf + 1) % NBUFS;
	buf = &allbufs[curbuf][0];

	if (prompt != NULL)
		printf("%s", prompt);

	i = 0;
	while (1) {
		c = getchar();
		if (c < 0) {
			if (c != -E_EOF)
				printf("read error: %s\n", e2s(c));
			ioctl(0, TCSETS, &term);
			return NULL;
		} else if (c >= ' ' && i < BUFLEN-1) {
			buf[i++] = c;
		}
		else if (c == '\b' && i > 0) {
			i--;
		}
		else if (c == '\n' || c == '\r') {
			buf[i] = 0;
			fflush(stdout);
			ioctl(0, TCSETS, &term);
			return buf;
		}
	}
}
