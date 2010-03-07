#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/jhexdump.h>

void
jhexdump(const unsigned char *buf, unsigned int len)
{
	unsigned int i, j;

	i = 0;
	while (i < len) {
		char offset[9];
		char hex[16][3];
		char ascii[17];

		snprintf(offset, sizeof(offset), "%08x  ", i);
		offset[sizeof(offset) - 1] = '\0';

		for (j = 0; j < 16; j++) {
			if ((i + j) >= len) {
				strcpy(hex[j], "  ");
				ascii[j] = '\0';
			} else {
				snprintf(hex[j], sizeof(hex[0]), "%02x",
				    buf[i + j]);
				hex[j][sizeof(hex[0]) - 1] = '\0';
				if (isprint((int)buf[i + j]))
					ascii[j] = buf[i + j];
				else
					ascii[j] = '.';
			}
		}
		ascii[sizeof(ascii) - 1] = '\0';

		cprintf("%s  %s %s %s %s %s %s %s %s  %s %s %s %s %s %s %s %s  "
		    "|%s|\n", offset, hex[0], hex[1], hex[2], hex[3], hex[4],
		    hex[5], hex[6], hex[7], hex[8], hex[9], hex[10], hex[11],
		    hex[12], hex[13], hex[14], hex[15], ascii);

		i += 16;
	}
}
