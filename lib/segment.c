#include <machine/x86.h>
#include <inc/memlayout.h>
#include <inc/lib.h>
#include <inc/setjmp.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/thread.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/jthread.h>
#include <inc/utrap.h>

#include <string.h>

#define N_BASE_MAPPINGS 64
static struct u_segment_mapping cache_ents[N_BASE_MAPPINGS];
static struct u_address_space cache_uas = { .size = N_BASE_MAPPINGS,
					    .ents = &cache_ents[0] };
static struct cobj_ref cache_asref;

static uint64_t	cache_thread_id;

static jthread_mutex_t as_mutex;

enum { segment_debug = 0 };

static void __attribute__((noinline))
reserve_stack_page(void)
{
    volatile char page[PGSIZE];
    page[0] = '\0';
}

static int __attribute__((warn_unused_result))
as_mutex_lock(void)
{
    void *rsp = (void *) read_rsp();
    if (setup_env_done && !utrap_is_masked() &&
	(rsp > tls_stack_top || rsp <= tls_base))
    {
	reserve_stack_page();
    }

    int old = utrap_set_mask(1);
    jthread_mutex_lock(&as_mutex);
    return old;
}

static void
as_mutex_unlock(int old)
{
    jthread_mutex_unlock(&as_mutex);
    utrap_set_mask(old);
}

static void
cache_uas_flush(void)
{
    if (!cache_asref.object)
	return;

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
cache_uas_grow(void)
{
    cache_invalidate();

    struct cobj_ref cur_as;
    int r = sys_self_get_as(&cur_as);
    if (r < 0)
	return r;

    r = sys_as_get(cur_as, &cache_uas);
    if (r < 0)
	return r;

    uint64_t nsize = cache_uas.size * 2;
    uint64_t nbytes = nsize * sizeof(cache_ents[0]);
    struct u_segment_mapping *usme =
	(struct u_segment_mapping *) USEGMAPENTS;

    uint64_t i;
    for (i = 0; i < cache_uas.nent; i++) {
	if (cache_uas.ents[i].flags && cache_uas.ents[i].va == (void *) usme) {
	    r = sys_segment_resize(cache_uas.ents[i].segment, nbytes);
	    if (r < 0)
		return r;

	    cache_uas.ents[i].num_pages = ROUNDUP(nbytes, PGSIZE) / PGSIZE;
	    r = sys_as_set_slot(cur_as, &cache_uas.ents[i]);
	    if (r < 0)
		return r;

	    break;
	}
    }

    if (i == cache_uas.nent) {
	if (i >= cache_uas.size)
	    return -E_NO_SPACE;

	int64_t id = sys_segment_create(start_env->proc_container,
					nbytes, 0, "segmap entries");
	if (id < 0)
	    return id;

	cache_uas.nent++;
	cache_uas.ents[i].segment = COBJ(start_env->proc_container, id);
	cache_uas.ents[i].start_page = 0;
	cache_uas.ents[i].num_pages = ROUNDUP(nbytes, PGSIZE) / PGSIZE;
	cache_uas.ents[i].flags = SEGMAP_READ | SEGMAP_WRITE;
	cache_uas.ents[i].va = (void *) usme;

	r = sys_as_set(cur_as, &cache_uas);
	if (r < 0) {
	    sys_obj_unref(cache_uas.ents[i].segment);
	    return r;
	}
    }

    cache_uas.ents = &usme[0];
    cache_uas.size = nsize;
    return 0;
}

static int
cache_refresh(struct cobj_ref as)
{
    int r;

    if (as.object == cache_asref.object)
	return 0;

    cache_uas_flush();
retry:
    r = sys_as_get(as, &cache_uas);
    if (r == -E_NO_SPACE) {
	r = cache_uas_grow();
	if (r < 0)
	    return r;
	goto retry;
    }

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

void
segment_as_invalidate_nowb(void)
{
    cache_asref.object = 0;
}

void
segment_map_print(struct u_address_space *as)
{
    cprintf("slot  kslot  segment  start  npages  fl  va\n");
    for (uint64_t i = 0; i < as->nent; i++) {
	char name[KOBJ_NAME_LEN];
	name[0] = '\0';
	sys_obj_get_name(as->ents[i].segment, &name[0]);

	if (as->ents[i].flags == 0)
	    continue;
	cprintf("%4ld  %5d  %3ld.%-3ld  %5ld  %6ld  %02x  %p (%s)\n",
		i, as->ents[i].kslot,
		as->ents[i].segment.container,
		as->ents[i].segment.object,
		as->ents[i].start_page,
		as->ents[i].num_pages,
		as->ents[i].flags,
		as->ents[i].va,
		name);
    }
}

int
segment_unmap_kslot(uint32_t kslot, int can_delay)
{
    int lockold = as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	if (cache_uas.ents[i].kslot == kslot &&
	    cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP))
	{
	    if (can_delay) {
		cache_uas.ents[i].flags |= SEGMAP_DELAYED_UNMAP;
		as_mutex_unlock(lockold);
		return 0;
	    }

	    if (segment_debug)
		cprintf("segment_unmap_kslot: kslot %d\n", kslot);

	    cache_uas.ents[i].flags = 0;
	    r = sys_as_set_slot(as_ref, &cache_uas.ents[i]);
	    if (r < 0)
		cache_invalidate();
	    as_mutex_unlock(lockold);
	    return r;
	}
    }

    as_mutex_unlock(lockold);
    return -E_INVAL;
}

int
segment_unmap_range(void *range_start, void *range_end, int can_delay)
{
    int lockold = as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	void *ent_start = cache_uas.ents[i].va;
	void *ent_end   = cache_uas.ents[i].va +
			  cache_uas.ents[i].num_pages * PGSIZE;

	if (ent_start >= range_start && ent_end <= range_end &&
	    cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP))
	{
	    if (can_delay) {
		cache_uas.ents[i].flags |= SEGMAP_DELAYED_UNMAP;
	    } else {
		cache_uas.ents[i].flags = 0;
		r = sys_as_set_slot(as_ref, &cache_uas.ents[i]);
		if (r < 0) {
		    cache_invalidate();
		    as_mutex_unlock(lockold);
		    return r;
		}
	    }
	}
    }

    as_mutex_unlock(lockold);
    return 0;
}

int
segment_unmap_delayed(void *va, int can_delay)
{
    int lockold = as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	if (cache_uas.ents[i].va == va &&
	    cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP))
	{
	    if (can_delay) {
		cache_uas.ents[i].flags |= SEGMAP_DELAYED_UNMAP;
		as_mutex_unlock(lockold);
		return 0;
	    }

	    if (segment_debug)
		cprintf("segment_unmap: va %p\n", cache_uas.ents[i].va);

	    cache_uas.ents[i].flags = 0;
	    r = sys_as_set_slot(as_ref, &cache_uas.ents[i]);
	    if (r < 0)
		cache_invalidate();
	    as_mutex_unlock(lockold);
	    return r;
	}
    }

    as_mutex_unlock(lockold);
    return -E_INVAL;
}

int
segment_unmap(void *va)
{
    return segment_unmap_delayed(va, 0);
}

int
segment_set_utrap(void *entry, void *stack_base, void *stack_top)
{
    int lockold = as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    cache_uas.trap_handler = entry;
    cache_uas.trap_stack_base = stack_base;
    cache_uas.trap_stack_top = stack_top;
    r = sys_as_set(as_ref, &cache_uas);
    if (r < 0) {
	cache_invalidate();
	as_mutex_unlock(lockold);
	return r;
    }

    as_mutex_unlock(lockold);
    return 0;
}

int
segment_lookup(void *va, struct u_segment_mapping *usm)
{
    return segment_lookup_skip(va, usm, 0);
}

int
segment_lookup_skip(void *va, struct u_segment_mapping *usm, uint64_t skip_flags)
{
    int lockold = as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	void *va_start = cache_uas.ents[i].va;
	void *va_end = cache_uas.ents[i].va + cache_uas.ents[i].num_pages * PGSIZE;
	if (cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP) &&
	    !(cache_uas.ents[i].flags & skip_flags) &&
	    va >= va_start && va < va_end)
	{
	    if (usm)
		*usm = cache_uas.ents[i];
	    as_mutex_unlock(lockold);
	    return 1;
	}
    }

    as_mutex_unlock(lockold);
    return 0;
}

int
segment_lookup_obj(uint64_t oid, struct u_segment_mapping *usm)
{
    int lockold = as_mutex_lock();

    struct cobj_ref as_ref;
    int r = self_get_as(&as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    for (uint64_t i = 0; i < cache_uas.nent; i++) {
	if (cache_uas.ents[i].flags &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP) &&
	    cache_uas.ents[i].segment.object == oid)
	{
	    if (usm)
		*usm = cache_uas.ents[i];
	    as_mutex_unlock(lockold);
	    return 1;
	}
    }

    as_mutex_unlock(lockold);
    return 0;
}

static int
segment_map_as_locked(struct cobj_ref as_ref, struct cobj_ref seg,
		      uint64_t start_byteoff, uint64_t flags,
		      void **va_p, uint64_t *bytes_store,
		      uint64_t map_opts, int lockold)
{
    assert((start_byteoff % PGSIZE) == 0);

    if (!(flags & SEGMAP_READ)) {
	cprintf("segment_map: unreadable mappings not supported\n");
	as_mutex_unlock(lockold);
	return -E_INVAL;
    }

    uint64_t seg_bytes;
    if (bytes_store && *bytes_store) {
	seg_bytes = *bytes_store + start_byteoff;
    } else {
	int64_t nbytes = sys_segment_get_nbytes(seg);
	if (nbytes < 0) {
	    cprintf("segment_map: cannot stat segment: %s\n", e2s(nbytes));
	    as_mutex_unlock(lockold);
	    return nbytes;
	}

	seg_bytes = nbytes;
    }

    int r;
    uint64_t map_bytes = ROUNDUP(seg_bytes - start_byteoff, PGSIZE);

cache_grown:
    r = cache_refresh(as_ref);
    if (r < 0) {
	as_mutex_unlock(lockold);
	cprintf("segment_map: cache_refresh: %s\n", e2s(r));
	return r;
    }

    if (cache_uas.nent >= cache_uas.size - 2) {
	r = cache_uas_grow();
	if (r < 0) {
	    as_mutex_unlock(lockold);
	    return r;
	}

	goto cache_grown;
    }

    char *map_start, *map_end;

    if (va_p && *va_p) {
	map_start = (char *) *va_p;
	map_end = map_start + map_bytes;

	assert((((uintptr_t) map_start) % PGSIZE) == 0);
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

	    // Delayed-unmap entries in general do not conflict; the logic
	    // further down will unmap anything that's in the way.
	    // We try to keep them intact in case we'll reuse them soon.
	    // However, for the same object, we want to reuse it!
	    if ((cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP) &&
		cache_uas.ents[i].segment.object == seg.object)
		continue;

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

	// If this segment is live and overlaps with our
	// non-reserved range, bail out.
	if (cache_uas.ents[i].flags &&
	    !(map_opts & SEG_MAPOPT_OVERLAP) &&
	    !(cache_uas.ents[i].flags & SEGMAP_DELAYED_UNMAP) &&
	    ent_start < map_end && ent_end > map_start)
	{
	    // Except that it's OK if the user asks for replacement
	    if ((map_opts & SEG_MAPOPT_REPLACE) &&
		cache_uas.ents[i].va == map_start &&
		cache_uas.ents[i].start_page == start_byteoff / PGSIZE &&
		match_segslot == cache_uas.nent)
	    {
		match_segslot = i;
	    } else {
		cprintf("segment_map: VA %p busy\n", map_start);
		cache_invalidate();
		as_mutex_unlock(lockold);
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
			as_mutex_unlock(lockold);
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
	    as_mutex_unlock(lockold);
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
    if (slot >= cache_uas.size) {
	cprintf("out of segment map slots\n");
	segment_map_print(&cache_uas);
	cache_invalidate();
	as_mutex_unlock(lockold);
	return -E_NO_MEM;
    }

    // Construct the mapping entry
    struct u_segment_mapping usm;
    usm.segment = seg;
    usm.start_page = start_byteoff / PGSIZE;
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
	as_mutex_unlock(lockold);
	goto out;
    }

    // Upload the slot into the kernel
    cache_uas.ents[slot] = usm;
    if (slot == cache_uas.nent)
	cache_uas.nent = slot + 1;

    if (segment_debug)
	cprintf("segment_map: mapping <%ld.%ld> at %p, slot %d\n",
		cache_uas.ents[slot].segment.container,
		cache_uas.ents[slot].segment.object,
		cache_uas.ents[slot].va, slot);

    r = sys_as_set_slot(as_ref, &cache_uas.ents[slot]);

    // Fall back to full sys_as_set() because sys_as_set_slot()
    // does not grow the address space (if we're out of free slots).
    if (r < 0) {
	if (segment_debug)
	    cprintf("segment_map: trying to grow address space\n");
	r = sys_as_set(as_ref, &cache_uas);
    }

    if (r < 0)
	cache_invalidate();

    as_mutex_unlock(lockold);
    if (r < 0) {
	cprintf("segment_map: kslot %d, %s\n", usm.kslot, e2s(r));
	return r;
    }

out:
    if (bytes_store)
	*bytes_store = seg_bytes - start_byteoff;
    if (va_p)
	*va_p = map_start;
    return 0;
}

int
segment_map(struct cobj_ref seg, uint64_t start_byteoff, uint64_t flags,
	    void **va_p, uint64_t *bytes_store, uint64_t map_opts)
{
    struct cobj_ref as;
    int lockold = as_mutex_lock();
    int r = self_get_as(&as);
    if (r < 0) {
	as_mutex_unlock(lockold);
	return r;
    }

    return segment_map_as_locked(as, seg, start_byteoff, flags,
				 va_p, bytes_store, map_opts, lockold);
}

int
segment_map_as(struct cobj_ref as_ref, struct cobj_ref seg,
	       uint64_t start_byteoff, uint64_t flags,
	       void **va_p, uint64_t *bytes_store,
	       uint64_t map_opts)
{
    int lockold = as_mutex_lock();
    return segment_map_as_locked(as_ref, seg, start_byteoff, flags,
				 va_p, bytes_store, map_opts, lockold);
}

int
segment_alloc(uint64_t container, uint64_t bytes,
	      struct cobj_ref *cobj, void **va_p,
	      const struct ulabel *label, const char *name)
{
    int64_t id = sys_segment_create(container, bytes, label, name);
    if (id < 0)
	return id;

    if (cobj)
	*cobj = COBJ(container, id);

    if (va_p) {
	uint64_t mapped_bytes = bytes;
	int r = segment_map(COBJ(container, id), 0, SEGMAP_READ | SEGMAP_WRITE,
			    va_p, &mapped_bytes, 0);
	if (r < 0) {
	    sys_obj_unref(COBJ(container, id));
	    return r;
	}
    }

    return 0;
}
