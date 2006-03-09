#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <inc/assert.h>

static void
as_print_ents(struct u_address_space *uas)
{
    cprintf("              va off npg rwx segment\n");
    for (uint32_t i = 0; i < uas->nent; i++) {
	if (uas->ents[i].flags == 0)
	    continue;

	uint64_t flags = uas->ents[i].flags;
	cprintf("%16lx %3ld %3ld %c%c%c %ld.%ld\n",
		(uint64_t) uas->ents[i].va,
		uas->ents[i].start_page,
		uas->ents[i].num_pages,
		(flags & SEGMAP_READ)  ? 'r' : '-',
		(flags & SEGMAP_WRITE) ? 'w' : '-',
		(flags & SEGMAP_EXEC)  ? 'x' : '-',
		uas->ents[i].segment.container,
		uas->ents[i].segment.object);
    }
}

#define NENT	64

static void
as_print(struct cobj_ref as)
{
    uint32_t usm_nent = 64;

    struct u_segment_mapping *ents = malloc(usm_nent * sizeof(*ents));
    if (ents == 0)
	panic("malloc ents");

    struct u_address_space uas = { .size = usm_nent, .ents = &ents[0] };

    int r = sys_as_get(as, &uas);
    if (r < 0) {
	printf("sys_as_get: %s\n", e2s(r));
	return;
    }

    as_print_ents(&uas);
}

static void
usage(const char *n)
{
    printf("Usage: %s container object\n", n);
}

int
main(int ac, char **av)
{
    if (ac != 3) {
	usage(av[0]);
	return -1;
    }

    struct cobj_ref as;
    if (strtou64(av[1], 0, 10, &as.container) < 0) {
	usage(av[0]);
	return -1;
    }

    if (strtou64(av[2], 0, 10, &as.object) < 0) {
	usage(av[0]);
	return -1;
    }

    as_print(as);
    return 0;
}
