#include <machine/memlayout.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/jthread.h>
#include <inc/stdio.h>

#include <unistd.h>
#include <inttypes.h>

static struct {
    uint64_t inited_pct;
    struct cobj_ref heapseg;
    jthread_mutex_t mu;
    size_t brk;
} heap;

static void *heap_base = (void *) UHEAP;
static uint64_t heap_maxbytes = UHEAPTOP - UHEAP;

void *
sbrk(intptr_t x)
{
    int r;
    void *p = 0;

    if (!start_env) {
	cprintf("sbrk called without a start_env\n");
	return 0;
    }

    jthread_mutex_lock(&heap.mu);

    if (heap.inited_pct != start_env->proc_container) {
	struct u_segment_mapping usm;
	r = segment_lookup(heap_base, &usm);
	if (r < 0) {
	    cprintf("sbrk: error in segment_lookup: %s\n", e2s(r));
	    goto out;
	}

	if (r > 0) {
	    heap.heapseg = usm.segment;
	} else {
	    r = segment_alloc(start_env->proc_container, 0,
			      &heap.heapseg, 0, 0, "heap");
	    if (r < 0) {
		cprintf("sbrk: cannot allocate heap: %s\n", e2s(r));
		goto out;
	    }

	    r = segment_map(heap.heapseg, 0, SEGMAP_READ | SEGMAP_WRITE,
			    &heap_base, &heap_maxbytes, 0);
	    if (r < 0) {
		sys_obj_unref(heap.heapseg);
		cprintf("sbrk: cannot map heap: %s\n", e2s(r));
		goto out;
	    }
	}

	heap.inited_pct = start_env->proc_container;
    }

    size_t nbrk = heap.brk + x;
    if (nbrk > heap_maxbytes) {
	cprintf("sbrk: heap too large: %zu > %"PRIu64"\n", nbrk, heap_maxbytes);
	goto out;
    }

    r = sys_segment_resize(heap.heapseg, nbrk);
    if (r < 0) {
	cprintf("sbrk: resizing heap to %zu: %s\n", nbrk, e2s(r));
	goto out;
    }

    p = heap_base + heap.brk;
    heap.brk = nbrk;

out:
    jthread_mutex_unlock(&heap.mu);
    return p;
}
