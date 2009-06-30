#include <inttypes.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <inc/syscall.h>

#include <sys/types.h>
#include <sys/stat.h>

int
main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s [irqno] [maxwaits]\n", argv[0]);
		exit(1);
	}

	int irqno = atoi(argv[1]);
	int maxwaits = atoi(argv[2]);
	int64_t last = sys_irq_wait(irqno, -1);
	printf("initial irq%d count: %" PRId64 "\n", irqno, last);
	for (int i = 0; i < maxwaits; i++) {
		printf("waiting on irq%d (last = %" PRId64 ")\n",
		    irqno, last);
		sys_irq_wait(irqno, last);
		int64_t newlast = sys_irq_wait(irqno, -1);
		fprintf(stderr, "  awoke: delta = %d irq(s)\n", newlast - last);
		last = newlast;
	}

	return (0);
}
