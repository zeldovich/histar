#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/error.h>

int
fs_get_root(uint64_t rc, struct cobj_ref *o)
{
    int64_t c_fs = sys_container_get_slot_id(rc, 0);
    if (c_fs < 0)
	return c_fs;

    int64_t dir_id = sys_container_get_slot_id(c_fs, 0);
    if (dir_id < 0)
	return dir_id;

    *o = COBJ(c_fs, dir_id);
    return 0;
}

int
fs_get_dent(struct cobj_ref d, int n, struct fs_dent *e)
{
    uint64_t *dirbuf = 0;
    int r = segment_map(d, SEGMAP_READ, (void**)&dirbuf, 0);
    if (r < 0)
	return r;

    int max_dent = dirbuf[0];

    // The directory entries start with 1, so offset the index by 1
    n++;

    if (n < 0 || n >= max_dent)
	return -E_RANGE;

    e->de_cobj = COBJ(d.container, dirbuf[16*n]);
    strcpy(e->de_name, (char*) &dirbuf[16*n+1]);

    r = segment_unmap(dirbuf);
    if (r < 0)
	panic("cannot unmap dirbuf: %s", e2s(r));

    return 0;
}

int
fs_lookup(struct cobj_ref d, const char *pn, struct cobj_ref *o)
{
    struct fs_dent de;
    int n = 0;

    for (;;) {
	int r = fs_get_dent(d, n++, &de);
	if (r == -E_RANGE)
	    return -E_NOT_FOUND;
	if (r < 0)
	    return r;

	if (!strcmp(pn, de.de_name)) {
	    *o = de.de_cobj;
	    return 0;
	}
    }
}
