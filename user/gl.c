#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/syscall.h>
#include <inc/error.h>

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

static void
usage(const char *n)
{
    printf("Usage: %s container object\n", n);
    printf("       %s pathname\n", n);
}

int
main(int ac, char **av)
{
    struct cobj_ref o;
    if (ac == 3) {
	if (strtou64(av[1], 0, 10, &o.container) < 0) {
	    usage(av[0]);
	    return -1;
	}

	if (strtou64(av[2], 0, 10, &o.object) < 0) {
	    usage(av[0]);
	    return -1;
	}
    } else if (ac == 2) {
	struct fs_inode fs_obj;
	int r = fs_namei(av[1], &fs_obj);
	if (r < 0) {
	    printf("Cannot find %s: %s\n", av[1], e2s(r));
	    usage(av[0]);
	    return -1;
	}

	o = fs_obj.obj;
    } else {
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
	printf("label for <%"PRIu64".%"PRIu64">: %s\n",
	       o.container, o.object, label_to_string(l));
    }

    int type = sys_obj_get_type(o);
    if (type >= 0 && type == kobj_gate) {
retry2:
	r = sys_gate_clearance(o, l);
	if (r == -E_NO_SPACE) {
	    r = label_grow(l);
	    if (r == 0)
		goto retry2;
	}

	if (r < 0) {
	    printf("cannot get clearance: %s\n", e2s(r));
	} else {
	    printf("clearance for <%"PRIu64".%"PRIu64">: %s\n",
		   o.container, o.object, label_to_string(l));
	}
    }
}
