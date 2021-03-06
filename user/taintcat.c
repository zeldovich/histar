#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <stdio.h>
#include <unistd.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s pathname\n", av[0]);
	return -1;
    }

    const char *pn = av[1];
    struct fs_inode f;
    int r = fs_namei(pn, &f);
    if (r < 0) {
	printf("cannot lookup %s: %s\n", pn, e2s(r));
	return r;
    }

    uint64_t maxoff;
    do {
	r = fs_getsize(f, &maxoff);
	if (r < 0) {
	    printf("cannot stat %s: %s\n", pn, e2s(r));
	    if (r == -E_LABEL) {
		int q = fs_taint_self(f);
		if (q < 0) {
		    printf("cannot taint self: %s\n", e2s(q));
		    return q;
		}
	    } else {
		return r;
	    }
	}
    } while (r == -E_LABEL);

    uint64_t off = 0;
    while (off < maxoff) {
	char buf[512];
	size_t cc = MIN(sizeof(buf), maxoff - off);

	ssize_t cr = fs_pread(f, &buf[0], cc, off);
	if (cr < 0) {
	    printf("cannot read %s: %s\n", pn, e2s(cr));
	    return cr;
	}

	write(1, buf, cr);
	off += cr;
    }

    return 0;
}
