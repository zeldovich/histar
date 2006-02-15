#include <machine/memlayout.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/mutex.h>

static struct {
    int inited;
    mutex_t mu;
    size_t brk;
    void *base;
    struct cobj_ref obj;
} heap;

void *
sbrk(intptr_t x)
{
    int r;
    void *p = 0;

    mutex_lock(&heap.mu);

    if (!heap.inited) {
	heap.base = (void *) UHEAP;
	r = segment_alloc(start_env->container, 0,
			  &heap.obj, &heap.base, 0, "heap");
	if (r < 0) {
	    printf("malloc: cannot allocate heap: %s\n", e2s(r));
	    goto out;
	}

	heap.inited = 1;
    }

    size_t nbrk = heap.brk + x;
    uint64_t npages = (nbrk + PGSIZE - 1) / PGSIZE;
    r = sys_segment_resize(heap.obj, npages);
    if (r < 0) {
	printf("malloc: resizing heap to %ld pages: %s\n", npages, e2s(r));
	goto out;
    }

    r = segment_map(heap.obj, SEGMAP_READ | SEGMAP_WRITE, &heap.base, 0);
    if (r < 0) {
	printf("malloc: mapping heap: %s\n", e2s(r));
	goto out;
    }

    p = heap.base + heap.brk;
    heap.brk = nbrk;

out:
    mutex_unlock(&heap.mu);
    return p;
}

int
heap_relabel(struct ulabel *l)
{
    int64_t r = 0;
    mutex_lock(&heap.mu);

    if (heap.inited) {
	r = sys_segment_copy(heap.obj, start_env->container, l, "heap");
	if (r < 0) {
	    printf("heap_relabel: cannot copy: %s\n", e2s(r));
	    goto out;
	}
	uint64_t nid = r;

	r = segment_unmap(heap.base);
	if (r < 0) {
	    printf("heap_relabel: cannot unmap: %s\n", e2s(r));
	    goto out;
	}

	sys_obj_unref(heap.obj);
	heap.obj = COBJ(start_env->container, nid);
	r = segment_map(heap.obj, SEGMAP_READ | SEGMAP_WRITE, &heap.base, 0);
	if (r < 0) {
	    printf("heap_relabel: cannot remap: %s\n", e2s(r));
	    goto out;
	}
    }

out:
    mutex_unlock(&heap.mu);
    return r;
}
