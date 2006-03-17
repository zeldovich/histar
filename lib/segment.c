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
static struct cobj_ref cache_asref;

static uint64_t	cache_thread_id;

static pthread_mutex_t as_mutex;

enum { segment_debug = 0 };

static void
as_mutex_lock(void) {
    pthread_mutex_lock(&as_mutex);
}

static void
as_mutex_unlock(void) {
    pthread_mutex_unlock(&as_mutex);
}

static void
cache_uas_flush(void)
{
    for (uint32_t i = 0; i < cache_uas.nent; i++) {
	if ((cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP)) {
	    cache_uas.ents[i].flags = 0;

	    if (segment_debug)
		cprintf("cache_uas_flush: va %p\n", cache_uas.ents[i].va);

	    int r = sys_as_set_slot(cache_asref, &cache_uas.ents[i]);
	    if (r < 0)
		panic("cache_uas_flush: writeback: %s", e2s(r));
	}
    }
}

static void
cache_invalidate(void)
{
    cache_uas_flush();
    cache_asref.object = 0;
}

static int
cache_refresh(struct cobj_ref as)
{
    if (as.object == cache_asref.object)
	return 0;

    int r = sys_as_get(as, &cache_uas);
    if (r < 0) {
	cache_invalidate();
	return r;
    }

    cache_asref = as;
    return 0;
}

void
segment_unmap_flush(void)
{
    cache_uas_flush();
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
    cprintf("slot  kslot  segment  start  npages  fl  va\n");
    for (uint64_t i = 0; i < as->nent; i++) {
	if (as->ents[i].flags == 0)
	    continue;
	cprintf("%4ld  %5d  %3ld.%-3ld  %5ld  %6ld  %02x  %p\n",
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
segment_unmap_delayed(void *va, int can_delay)
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
	if (cache_uas.ents[i].va == va &&
	    cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP))
	{
	    if (can_delay) {
		cache_uas.ents[i].flags |= SEGMAP_DELAYED_UNMAP;
		as_mutex_unlock();
		return 0;
	    }

	    if (segment_debug)
		cprintf("segment_unmap: va %p\n", cache_uas.ents[i].va);

	    cache_uas.ents[i].flags = 0;
	    r = sys_as_set_slot(as_ref, &cache_uas.ents[i]);
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
segment_unmap(void *va)
{
    return segment_unmap_delayed(va, 0);
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
	if (cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP) &&
	    va >= va_start && va < va_end)
	{
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
	if (cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP) &&
	    cache_uas.ents[i].segment.object == oid)
	{
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
    if (nbytes < 0) {
	cprintf("segment_map: cannot stat segment: %s\n", e2s(nbytes));
	return nbytes;
    }

    uint64_t map_bytes = ROUNDUP(nbytes, PGSIZE);

    as_mutex_lock();
    int r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock();
	cprintf("segment_map: cache_refresh: %s\n", e2s(r));
	return r;
    }

    char *map_start, *map_end;

    if (va_p && *va_p) {
	map_start = (char *) *va_p;
	map_end = map_start + map_bytes;
    } else {
	// If we don't have a fixed address, try to find a free one.
	map_start = (char *) UMMAPBASE;

retry:
	map_end = map_start + map_bytes;

	for (uint64_t i = 0; i < cache_uas.nent; i++) {
	    if (!cache_uas.ents[i].flags)
		continue;

	    char *ent_start = cache_uas.ents[i].va;
	    char *ent_end = ent_start + cache_uas.ents[i].num_pages * PGSIZE;

	    // Leave page gaps between mappings for good measure
	    if (ent_start <= map_end && ent_end >= map_start) {
		map_start = ent_end + PGSIZE;
		goto retry;
	    }
	}
    }

    // Now try to map the segment at [map_start .. map_end)
    // We must scan the list of mapped segments again, and:
    //  * Check that the range does not overlap any live segments,
    //  * Flush out any overlapping delayed-unmap segmets.

    // While scanning, keep track of what slot we can use for the mapping.
    uint32_t match_segslot = cache_uas.nent;	// Matching live slot
    uint32_t delay_overlap = cache_uas.nent;	// Overlapping
    uint32_t delay_segslot = cache_uas.nent;	// Non-overlapping
    uint32_t empty_segslot = cache_uas.nent;	// Not delay-unmapped

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	char *ent_start = cache_uas.ents[i].va;
	char *ent_end = ent_start + cache_uas.ents[i].num_pages * PGSIZE;

	// If this segment is live and overlaps with our range, bail out.
	if (cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP) &&
	    ent_start < map_end && ent_end > map_start)
	{
	    // Except that it's OK if it's the same exact segment?
	    if (cache_uas.ents[i].segment.object == seg.object &&
		cache_uas.ents[i].va == map_start &&
		cache_uas.ents[i].start_page == 0 &&
		match_segslot == cache_uas.nent)
	    {
		match_segslot = i;
	    } else {
		cprintf("segment_map: VA %p busy\n", map_start);
		cache_invalidate();
		as_mutex_unlock();
		return -E_BUSY;
	    }
	}

	if (!cache_uas.ents[i].flags)
	    empty_segslot = i;

	if ((cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP)) {
	    delay_segslot = i;

	    if (ent_start < map_end && ent_end > map_start) {
		if (delay_overlap == cache_uas.nent) {
		    delay_overlap = i;
		} else {
		    // Multiple delay-unmapped segments; must force unmap.
		    cache_uas.ents[i].flags = 0;

		    if (segment_debug)
			cprintf("segment_map: overlap 1, va %p\n",
				cache_uas.ents[i].va);

		    r = sys_as_set_slot(as_ref, &cache_uas.ents[i]);
		    if (r < 0) {
			cprintf("segment_map: flush unmap: %s\n", e2s(r));
			cache_invalidate();
			as_mutex_unlock();
			return r;
		    }
		}
	    }
	}
    }

    // If we found an exact live match and an overlapping delayed-unmapping,
    // we need to free the delayed unmapping.
    if (match_segslot != cache_uas.nent && delay_overlap != cache_uas.nent) {
	cache_uas.ents[delay_overlap].flags = 0;

	if (segment_debug)
	    cprintf("segment_map: overlap 2, va %p\n",
		    cache_uas.ents[delay_overlap].va);

	r = sys_as_set_slot(as_ref, &cache_uas.ents[delay_overlap]);
	if (r < 0) {
	    cprintf("segment_map: flush unmap 2: %s\n", e2s(r));
	    cache_invalidate();
	    as_mutex_unlock();
	    return r;
	}
    }

    // Figure out which slot to use.
    uint32_t slot = empty_segslot;
    if (delay_segslot != cache_uas.nent)
	slot = delay_segslot;
    if (delay_overlap != cache_uas.nent)
	slot = delay_overlap;
    if (match_segslot != cache_uas.nent)
	slot = match_segslot;

    if (segment_debug)
	cprintf("segment_map: nent %ld empty %d delay %d overlap %d exact %d\n",
		cache_uas.nent, empty_segslot, delay_segslot,
		delay_overlap, match_segslot);

    // Make sure we aren't out of slots
    if (slot >= NMAPPINGS) {
	cprintf("out of segment map slots\n");
	cache_invalidate();
	segment_map_print(&cache_uas);
	as_mutex_unlock();
	return -E_NO_MEM;
    }

    // Construct the mapping entry
    struct u_segment_mapping usm;
    usm.segment = seg;
    usm.start_page = 0;
    usm.num_pages = map_bytes / PGSIZE;
    usm.flags = flags;
    usm.va = map_start;

    // If we're going to be replacing an existing entry,
    // reuse its kernel slot.
    if (slot < cache_uas.nent && cache_uas.ents[slot].flags) {
	usm.kslot = cache_uas.ents[slot].kslot;
    } else {
	// Find a free kernel slot.
	usm.kslot = 0;
	int conflict;
	do {
	    conflict = 0;
	    for (uint32_t i = 0; i < cache_uas.nent; i++) {
		if (cache_uas.ents[i].flags &&
		    cache_uas.ents[i].kslot == usm.kslot)
		{
		    usm.kslot++;
		    conflict++;
		}
	    }
	} while (conflict);
    }

    // See if we need to do any actual work
    cache_uas.ents[slot].flags &= ~SEGMAP_DELAYED_UNMAP;
    if (slot < cache_uas.nent &&
	!memcmp(&cache_uas.ents[slot], &usm, sizeof(usm)))
    {
	as_mutex_unlock();
	return 0;
    }

    // Upload the slot into the kernel
    cache_uas.ents[slot] = usm;
    if (slot == cache_uas.nent)
	cache_uas.nent = slot + 1;

    if (segment_debug)
	cprintf("segment_map: mapping <%ld.%ld> at %p\n",
		cache_uas.ents[slot].segment.container,
		cache_uas.ents[slot].segment.object,
		cache_uas.ents[slot].va);

    r = sys_as_set_slot(as_ref, &cache_uas.ents[slot]);

    // Fall back to full sys_as_set() because sys_as_set_slot()
    // does not grow the address space (if we're out of free slots).
    if (r < 0) {
	cprintf("segment_map: trying to grow address space\n");
	r = sys_as_set(as_ref, &cache_uas);
    }

    if (r < 0)
	cache_invalidate();

    as_mutex_unlock();
    if (r < 0) {
	cprintf("segment_map: kslot %d, %s\n", usm.kslot, e2s(r));
	return r;
    }

    if (bytes_store)
	*bytes_store = nbytes;
    if (va_p)
	*va_p = map_start;
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
