#include <stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <string.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    if (ac != 3) {
	printf("Usage: %s src dst\n", av[0]);
	return -1;
    }

    char *src_pn = av[1];
    char *dst_pn = av[2];

    struct fs_inode src;
    int r = fs_namei(src_pn, &src);
    if (r < 0) {
	printf("cannot lookup %s: %s\n", src_pn, e2s(r));
	return r;
    }

    uint64_t maxoff;
    do {
	r = fs_getsize(src, &maxoff);
	if (r < 0) {
	    printf("cannot stat %s: %s\n", src_pn, e2s(r));
	    if (r == -E_LABEL) {
		int q = fs_taint_self(src);
		if (q < 0) {
		    printf("cannot taint self: %s\n", e2s(q));
		    return q;
		}
	    } else {
		return r;
	    }
	}
    } while (r == -E_LABEL);

    const char *dir, *fn;
    fs_dirbase(dst_pn, &dir, &fn);

    struct fs_inode dst_dir;
    r = fs_namei(dir, &dst_dir);
    if (r < 0) {
	printf("cannot lookup destination directory %s: %s\n", dir, e2s(r));
	return r;
    }

    struct fs_inode dst;
    r = fs_create(dst_dir, fn, &dst);
    if (r < 0) {
	printf("cannot create destination file: %s\n", e2s(r));
	return r;
    }

    uint64_t off = 0;
    while (off < maxoff) {
	char buf[512];
	size_t cc = MIN(sizeof(buf), maxoff - off);

	ssize_t cr = fs_pread(src, &buf[0], cc, off);
	if (cr < 0) {
	    printf("cannot read %s: %s\n", src_pn, e2s(cr));
	    return cr;
	}

	cr = fs_pwrite(dst, &buf[0], cr, off);
	if (cr < 0) {
	    printf("cannot write: %s\n", e2s(cr));
	    return cr;
	}

	off += cr;
    }

    return 0;
}
