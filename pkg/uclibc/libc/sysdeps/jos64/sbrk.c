#include <machine/memlayout.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/pthread.h>
#include <inc/stdio.h>

#include <unistd.h>

static struct {
    int inited;
    pthread_mutex_t mu;
    size_t brk;
} heap;

static void *heap_base = (void *) UHEAP;

static int
find_heap(struct cobj_ref *o)
{
    int64_t id = container_find(start_env->proc_container, kobj_segment, "heap");
    if (id < 0)
	return id;

    *o = COBJ(start_env->proc_container, id);
    return 0;

    //return segment_lookup(heap_base, o, 0);
}

void *
sbrk(intptr_t x)
{
    int r;
    void *p = 0;

    pthread_mutex_lock(&heap.mu);

    struct cobj_ref heapobj;
    if (!heap.inited) {
	r = find_heap(&heapobj);
	if (r < 0)
	    r = segment_alloc(start_env->proc_container, 0,
			      &heapobj, &heap_base, 0, "heap");
	if (r < 0) {
	    cprintf("sbrk: cannot allocate heap: %s\n", e2s(r));
	    goto out;
	}

	heap.inited = 1;
    } else {
	r = find_heap(&heapobj);
	if (r < 0) {
	    cprintf("sbrk: cannot find heap: %s\n", e2s(r));
	    goto out;
	}
    }

    size_t nbrk = heap.brk + x;
    r = sys_segment_resize(heapobj, nbrk);
    if (r < 0) {
	cprintf("sbrk: resizing heap to %ld: %s\n", nbrk, e2s(r));
	goto out;
    }

    r = segment_map(heapobj, 0, SEGMAP_READ | SEGMAP_WRITE, &heap_base, 0, SEG_MAPOPT_REPLACE);
    if (r < 0) {
	cprintf("sbrk: mapping heap: %s\n", e2s(r));
	goto out;
    }

    p = heap_base + heap.brk;
    heap.brk = nbrk;

out:
    pthread_mutex_unlock(&heap.mu);
    return p;
}

int
heap_relabel(struct ulabel *l)
{
    int64_t r = 0;
    pthread_mutex_lock(&heap.mu);

    if (heap.inited) {
	struct cobj_ref heapobj;
	r = find_heap(&heapobj);
	if (r < 0) {
	    cprintf("heap_relabel: cannot find heap: %s\n", e2s(r));
	    goto out;
	}

	r = sys_segment_copy(heapobj, start_env->proc_container, l, "heap");
	if (r < 0) {
	    cprintf("heap_relabel: cannot copy: %s\n", e2s(r));
	    goto out;
	}
	uint64_t nid = r;

	struct cobj_ref newheap = COBJ(start_env->proc_container, nid);
	r = segment_map(newheap, 0, SEGMAP_READ | SEGMAP_WRITE,
			&heap_base, 0, SEG_MAPOPT_REPLACE);
	if (r < 0) {
	    cprintf("heap_relabel: cannot remap: %s\n", e2s(r));
	    sys_obj_unref(newheap);
	    goto out;
	}

	sys_obj_unref(heapobj);
    }

out:
    pthread_mutex_unlock(&heap.mu);
    return r;
}
