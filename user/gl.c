#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/error.h>

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

    struct cobj_ref o;
    if (strtoull(av[1], 0, 10, &o.container) < 0) {
	usage(av[0]);
	return -1;
    }

    if (strtoull(av[2], 0, 10, &o.object) < 0) {
	usage(av[0]);
	return -1;
    }

    struct ulabel *l = label_alloc();
    int r;

retry:
    r = sys_obj_get_label(o, l);
    if (r == -E_NO_SPACE) {
	r = label_grow(l);
	if (r == 0)
	    goto retry;
    }

    if (r < 0) {
	printf("cannot get label: %s\n", e2s(r));
    } else {
	printf("label for <%ld.%ld>: %s\n",
	       o.container, o.object, label_to_string(l));
    }
}
