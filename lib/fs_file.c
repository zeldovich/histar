#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/memlayout.h>

int
fs_pwrite(struct fs_inode f, const void *buf, uint64_t count, uint64_t off)
{
    uint64_t cursize;
    int r = fs_getsize(f, &cursize);
    if (r < 0)
	return r;

    uint64_t endpt = off + count;
    if (endpt > cursize)
	sys_segment_resize(f.obj, (endpt + PGSIZE - 1) / PGSIZE);

    char *map = 0;
    r = segment_map(f.obj, SEGMAP_READ | SEGMAP_WRITE, (void **) &map, 0);
    if (r < 0)
	return r;

    memcpy(&map[off], buf, count);
    segment_unmap(map);

    return 0;
}

int
fs_pread(struct fs_inode f, void *buf, uint64_t count, uint64_t off)
{
    uint64_t cursize;
    int r = fs_getsize(f, &cursize);
    if (r < 0)
	return r;

    uint64_t endpt = off + count;
    if (endpt > cursize)
	return -E_RANGE;

    char *map = 0;
    r = segment_map(f.obj, SEGMAP_READ, (void **) &map, 0);
    if (r < 0)
	return r;

    memcpy(buf, &map[off], count);
    segment_unmap(map);

    return 0;
}

int
fs_getsize(struct fs_inode f, uint64_t *len)
{
    int64_t npages = sys_segment_get_npages(f.obj);
    if (npages < 0)
	return npages;

    *len = npages * PGSIZE;
    return 0;
}
