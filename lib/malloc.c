#include <machine/memlayout.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/atomic.h>

/*
 * A rather pathetic malloc implementation.
 */

static struct {
    int inited;
    atomic_t mu;
    size_t brk;
    void *base;
    struct cobj_ref obj;
} heap;

void
free(void *ptr)
{
    // XXX this is why it's pathetic
}

void *
malloc(size_t size)
{
    int r;
    void *p = 0;

    while (atomic_compare_exchange(&heap.mu, 0, 1) != 0)
	sys_thread_yield();

    if (!heap.inited) {
	heap.base = (void *) UHEAP;
	r = segment_alloc(start_env->container, 0,
			  &heap.obj, &heap.base);
	if (r < 0) {
	    printf("malloc: cannot allocate heap: %s\n", e2s(r));
	    goto out;
	}

	sys_obj_set_name(heap.obj, "heap");
	heap.inited = 1;
    }

    size = ROUNDUP(size, 8);
    size_t nbrk = heap.brk + size;
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
    atomic_set(&heap.mu, 0);
    return p;
}
