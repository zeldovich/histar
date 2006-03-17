#include <inc/memlayout.h>
#include <inc/lib.h>
#include <inc/setjmp.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/thread.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/pthread.h>

#include <string.h>

#define NMAPPINGS 64
static struct u_segment_mapping cache_ents[NMAPPINGS];
static struct u_address_space cache_uas = { .size = NMAPPINGS,
					    .ents = &cache_ents[0] };
static uint64_t cache_asid;

static uint64_t	cache_thread_id;

static pthread_mutex_t as_mutex;

static void
as_mutex_lock(void) {
    pthread_mutex_lock(&as_mutex);
}

static void
as_mutex_unlock(void) {
    pthread_mutex_unlock(&as_mutex);
}

static int
cache_refresh(struct cobj_ref as)
{
    if (as.object == cache_asid)
	return 0;

    int r = sys_as_get(as, &cache_uas);
    if (r < 0) {
	cache_asid = 0;
	return r;
    }

    cache_asid = as.object;
    return 0;
}

static void
cache_invalidate(void)
{
    cache_asid = 0;
}

static int
self_get_as(struct cobj_ref *refp)
{
    static struct cobj_ref cached_thread_as;

    uint64_t tid = thread_id();
    if (tid != cache_thread_id) {
	int r = sys_self_get_as(&cached_thread_as);
	if (r < 0)
	    return r;

	cache_thread_id = tid;
    }

    *refp = cached_thread_as;
    return 0;
}

void
segment_as_switched(void)
{
    cache_thread_id = 0;
}

static void
segment_map_print(struct u_address_space *as)
{
    cprintf("slot  kslot  segment  start  npages  f  va\n");
    for (uint64_t i = 0; i < as->nent; i++) {
	if (as->ents[i].flags == 0)
	    continue;
	cprintf("%4ld  %5d  %3ld.%-3ld  %5ld  %6ld  %d  %p\n",
		i, as->ents[i].kslot,
		as->ents[i].segment.container,
		as->ents[i].segment.object,
		as->ents[i].start_page,
		as->ents[i].num_pages,
		as->ents[i].flags,
		as->ents[i].va);
    }
}

int
segment_unmap(void *va)
{
    as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock();
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock();
	return r;
    }

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	if (cache_uas.ents[i].va == va && cache_uas.ents[i].flags) {
	    cache_uas.ents[i].flags = 0;
	    r = sys_as_set_slot(as_ref, &cache_uas.ents[i]);
	    if (r < 0)
		r = sys_as_set(as_ref, &cache_uas);
	    if (r < 0)
		cache_invalidate();
	    as_mutex_unlock();
	    return r;
	}
    }

    as_mutex_unlock();
    return -E_INVAL;
}

int
segment_lookup(void *va, struct cobj_ref *seg, uint64_t *npage, uint64_t *flagsp)
{
    as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock();
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock();
	return r;
    }

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	void *va_start = cache_uas.ents[i].va;
	void *va_end = cache_uas.ents[i].va + cache_uas.ents[i].num_pages * PGSIZE;
	if (cache_uas.ents[i].flags && va >= va_start && va < va_end) {
	    if (seg)
		*seg = cache_uas.ents[i].segment;
	    if (npage)
		*npage = (va - va_start) / PGSIZE;
	    if (flagsp)
		*flagsp = cache_uas.ents[i].flags;
	    as_mutex_unlock();
	    return 1;
	}
    }

    as_mutex_unlock();
    return 0;
}

int
segment_lookup_obj(uint64_t oid, void **vap)
{
    as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock();
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock();
	return r;
    }

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	if (cache_uas.ents[i].flags && cache_uas.ents[i].segment.object == oid) {
	    if (vap)
		*vap = cache_uas.ents[i].va;
	    as_mutex_unlock();
	    return 1;
	}
    }

    as_mutex_unlock();
    return 0;
}

int
segment_map(struct cobj_ref seg, uint64_t flags,
	    void **va_p, uint64_t *bytes_store)
{
    struct cobj_ref as;
    as_mutex_lock();
    int r = self_get_as(&as);
    as_mutex_unlock();
    if (r < 0)
	return r;

    return segment_map_as(as, seg, flags, va_p, bytes_store);
}

int
segment_map_as(struct cobj_ref as_ref, struct cobj_ref seg,
	       uint64_t flags, void **va_p, uint64_t *bytes_store)
{
    if (!(flags & SEGMAP_READ)) {
	cprintf("segment_map: unreadable mappings not supported\n");
	return -E_INVAL;
    }

    int64_t nbytes = sys_segment_get_nbytes(seg);
    if (nbytes < 0)
	return nbytes;
    uint64_t map_bytes = ROUNDUP(nbytes, PGSIZE);

    as_mutex_lock();
    int r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock();
	return r;
    }

    int slot_optimize = 1;
    uint32_t free_segslot = cache_uas.nent;
    uint32_t free_kslot = 0;
    char *va_start = (char *) UMMAPBASE;
    char *va_end;

    int fixed_va = 0;
    if (va_p && *va_p) {
	fixed_va = 1;
	va_start = *va_p;

	if (va_start >= (char *) ULIM) {
	    cprintf("segment_map: VA %p over ulim\n", va_start);
	    as_mutex_unlock();
	    return -E_INVAL;
	}
    }

retry:
    va_end = va_start + map_bytes;
    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	// If it's the same segment we're trying to map, allow remapping
	if (fixed_va && cache_uas.ents[i].flags &&
	    cache_uas.ents[i].segment.object == seg.object &&
	    cache_uas.ents[i].va == va_start &&
	    cache_uas.ents[i].start_page == 0)
	{
	    cache_uas.ents[i].flags = 0;
	    slot_optimize = 0;
	}

	if (cache_uas.ents[i].flags == 0) {
	    free_segslot = i;
	    continue;
	}

	if (cache_uas.ents[i].kslot >= free_kslot)
	    free_kslot = cache_uas.ents[i].kslot + 1;

	char *m_start = cache_uas.ents[i].va;
	char *m_end = m_start + cache_uas.ents[i].num_pages * PGSIZE;

	// Leave unmapped gaps between mappings, when possible
	if (!fixed_va && m_start <= va_end && m_end >= va_start) {
	    va_start = m_end + PGSIZE;
	    goto retry;
	}

	if (fixed_va && m_start < va_end && m_end > va_start) {
	    cprintf("segment_map: fixed VA %p busy\n", va_start);
	    as_mutex_unlock();
	    return -E_NO_MEM;
	}
    }

    if (!fixed_va && va_end >= (char*) USTACKTOP) {
	cprintf("out of virtual address space!\n");
	as_mutex_unlock();
	return -E_NO_MEM;
    }

    if (free_segslot >= NMAPPINGS) {
	cprintf("out of segment map slots\n");
	segment_map_print(&cache_uas);
	as_mutex_unlock();
	return -E_NO_MEM;
    }

    cache_uas.ents[free_segslot].segment = seg;
    cache_uas.ents[free_segslot].start_page = 0;
    cache_uas.ents[free_segslot].num_pages = map_bytes / PGSIZE;
    cache_uas.ents[free_segslot].flags = flags;
    cache_uas.ents[free_segslot].va = va_start;
    cache_uas.ents[free_segslot].kslot = free_kslot;

    if (free_segslot == cache_uas.nent)
	cache_uas.nent = free_segslot + 1;

    if (slot_optimize)
	r = sys_as_set_slot(as_ref, &cache_uas.ents[free_segslot]);
    else
	r = -1;

    if (r < 0)
	r = sys_as_set(as_ref, &cache_uas);
    if (r < 0)
	cache_invalidate();

    as_mutex_unlock();
    if (r < 0)
	return r;

    if (bytes_store)
	*bytes_store = nbytes;
    if (va_p)
	*va_p = va_start;
    return 0;
}

int
segment_alloc(uint64_t container, uint64_t bytes,
	      struct cobj_ref *cobj, void **va_p,
	      struct ulabel *label, const char *name)
{
    int64_t id = sys_segment_create(container, bytes, label, name);
    if (id < 0)
	return id;

    *cobj = COBJ(container, id);

    if (va_p) {
	uint64_t mapped_bytes;
	int r = segment_map(*cobj, SEGMAP_READ | SEGMAP_WRITE,
			    va_p, &mapped_bytes);
	if (r < 0) {
	    sys_obj_unref(*cobj);
	    return r;
	}

	if (mapped_bytes != bytes) {
	    segment_unmap(*va_p);
	    sys_obj_unref(*cobj);
	    return -E_AGAIN;	// race condition maybe..
	}
    }

    return 0;
}
