#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <inc/error.h>
#include <inc/safeint.h>
#include <machine/arm.h>
#include <machine/cpu.h>
#include <machine/mmu.h>
#include <machine/pmap.h>

/*
 * Kernel Pagemap. Initialised in locore.S to map according to memlayout.h.
 *
 * This also serves as a prototype for future user page tables.
 */
struct Pagemap kpagemap __attribute__ ((aligned(16384), section(".data")));

/*
 * We need to keep some per-pte metadata. Unfortunately, there aren't spare
 * bits in the pte entries themselves and we cannot use pageinfo, since those
 * physical pages may be shared across mappings. So, we maintain a separate
 * page table per pgmap that holds ancilliary pte descriptors. This descriptor
 * reference is then placed in the pageinfo page for the pgmap.
 *
 * We'll do one uint8_t (pvp_t) per page; that's 256 indirect pages holding
 * 4096 bytes, each refering to a 4k page. Since we only have 2GB user AS's,
 * at absolute worst we'll have 129 * 4096 ~= 512KB overhead per pgmap.
 */
#define PVP_DIRTYEMU	0x1	/* emulate dirty bit for pte (actually is r/w)*/
#define PVP_DIRTYBIT	0x2	/* dirty bit set for this pte (page dirtied) */

typedef uint8_t pvp_t;

// Get our pvp_t pgmap virtual page descriptor associated with page *vp.
// If `create' is set, allocate any resources needed along the way.
static int pvp_get(const struct Pagemap *, const void *, pvp_t **, int)
    __attribute__ ((warn_unused_result));

#define	__UNCONST(_x)	((void *)(uintptr_t)(const void *)(_x))
static int
pvp_get(const struct Pagemap *pgmap, const void *va, pvp_t **storep, int create)
{
	struct page_info *pi = page_to_pageinfo(__UNCONST(pgmap));
	struct md_page_info *mpi = &pi->pi_md;
	int idx1 = (uintptr_t)va >> 24;
	int idx2 = ((uintptr_t)va >> 12) & 4095;
	int r;

	assert(storep != NULL);
	*storep = NULL;

	if (mpi->mpi_pmap_pvp == NULL) {
		uint8_t **p;

		if (!create)
			return (-E_INVAL);

		r = page_alloc((void **)&p);
		if (r < 0)
			return (r);

		memset(p, 0, PGSIZE);
		mpi->mpi_pmap_pvp = p;
	}

	if (mpi->mpi_pmap_pvp[idx1] == NULL) {
		uint8_t *p;

		if (!create)
			return (-E_INVAL);

		r = page_alloc((void **)&p);
		if (r < 0)
			return (r);

		memset(p, 0, PGSIZE);
		mpi->mpi_pmap_pvp[idx1] = p;
	}

	*storep = &mpi->mpi_pmap_pvp[idx1][idx2];
	return (0);
}
#undef __UNCONST

static void
pvp_free(struct Pagemap *pgmap)
{
	struct page_info *pi = page_to_pageinfo(pgmap);
	struct md_page_info *mpi = &pi->pi_md;

	if (mpi->mpi_pmap_pvp != NULL) {
		int i;

		for (i = 0; i < 256; i++) {
			if (mpi->mpi_pmap_pvp[i] != NULL)
				page_free(mpi->mpi_pmap_pvp[i]);
		}

		page_free(mpi->mpi_pmap_pvp);
	}

	mpi->mpi_pmap_pvp = NULL;
}

/*
 * Since L2 tables are 1K each, we maintain a free list and allocate 4 at
 * a time from the page allocator when necessary. The free list is maintained
 * per-Pagemap, rather than globally, so that we'll have more than a snowball's
 * chance in hell of returning a full page to the free list.
 *
 * We don't have room for this in our Pagemap (not without spilling to another
 * page in size!), so we'll use the MD portion of the page_info structure.
 */
static void
pmap_l2_free(struct Pagemap *pgmap, void *kva)
{
	struct page_info *pi = page_to_pageinfo(pgmap);
	uintptr_t *link = kva;

	if (PTE_ADDR((uintptr_t)kva) != (uintptr_t)kva)
		panic("%s: not a pte-aligned pointer %p", __func__, kva);

	*link = (uintptr_t)pi->pi_md.mpi_pmap_free_list;
	pi->pi_md.mpi_pmap_free_list = kva;
}

static int
pmap_l2_zalloc(struct Pagemap *pgmap, void **vp)
{
	struct page_info *pi = page_to_pageinfo(pgmap);
	uintptr_t *fl = pi->pi_md.mpi_pmap_free_list;
	void *subpage;

	if (fl == NULL) {
		uint8_t *p;
		int r, i;

		r = page_alloc((void **)&p);
		if (r < 0)
			return (r);

		for (i = 0; i < (PGSIZE / L2_PT_SIZE); i += L2_PT_SIZE)
			pmap_l2_free(pgmap, &p[i]);
	}

	fl = pi->pi_md.mpi_pmap_free_list;
	assert(fl != NULL);
	pi->pi_md.mpi_pmap_free_list = (void *)*(uintptr_t *)fl;

	subpage = fl;
	memset(subpage, 0, L2_PT_SIZE);
	*vp = subpage;

	assert(((uintptr_t)*vp & (ARM_MMU_L2_ALIGNMENT - 1)) == 0);

	return (0);
}

// Allocate a new L1 page table descriptor and initialise it using the
// kernel map to have the kernel mapped into the upper 2GB.
int
page_map_alloc(struct Pagemap **pm_store)
{
	void *pgmap;
	int r;

	r = page_alloc_n(&pgmap, L1_PT_SIZE / PGSIZE, ARM_MMU_L1_ALIGNMENT);
	if (r < 0)
		return (r);

	assert(((uintptr_t)pgmap & (ARM_MMU_L1_ALIGNMENT - 1)) == 0);

	page_to_pageinfo(pgmap)->pi_md.mpi_pmap_pvp = NULL;
	page_to_pageinfo(pgmap)->pi_md.mpi_pmap_free_list = NULL;
	memcpy(pgmap, &kpagemap, L1_PT_SIZE);
	cpufunc.cf_dcache_flush_invalidate_range(pgmap, L1_PT_SIZE);
	*pm_store = (struct Pagemap *)pgmap;

	return (0);
}

// Free the given L1 page table descriptor, as well as any L2 descriptors,
// recursively.
void
page_map_free(struct Pagemap *pgmap)
{
	// Skip top half where kernel lives under 1MB section entries.
	uintptr_t *fl, *pl;
	struct page_info *pi = page_to_pageinfo(pgmap);
	int maxi = NL1PTENTRIES / 2;
	int i;

	assert(kva2pa(pgmap) != cp15_ttbr_get());

	for (i = 0; i < maxi; i++) {
		ptent_t ptent = pgmap->pm_ent[i];

		if (ARM_MMU_L1_TYPE(ptent) == ARM_MMU_L1_TYPE_COARSE)
			pmap_l2_free(pgmap, pa2kva(PTE_ADDR(ptent)));
	}

	// truly free l2 subpages
	for (fl = pi->pi_md.mpi_pmap_free_list, pl = NULL; fl != NULL; ) {
		uintptr_t next = *fl;

		if (((uintptr_t)fl & (ARM_MMU_L2_ALIGNMENT - 1)) != 0) {
			fl = (uintptr_t *)next;
			continue;
		}

		*fl = (uintptr_t)pl;
		pl = fl;
		fl = (uintptr_t *)next;
	}
	for (fl = pl; fl != NULL; ) {
		uintptr_t next = *fl;
		page_free(fl);
		fl = (uintptr_t *)next;
	}

	pvp_free(pgmap);
	page_free_n(pgmap, L1_PT_SIZE / PGSIZE);
}

enum { pmap_invlpg_max = 4 };
static uint32_t cur_pgmap_invlpg_count;
static void *cur_pgmap_invlpg_addrs[pmap_invlpg_max];

static void
pmap_queue_invlpg(const struct Pagemap *pgmap, void *addr)
{
	if (cur_pgmap != pgmap)
		return;

	if (cur_pgmap_invlpg_count >= pmap_invlpg_max) {
		cur_pgmap_invlpg_count = pmap_invlpg_max + 1;
		return;
	}

	if (cur_pgmap_invlpg_count > 0 &&
	    cur_pgmap_invlpg_addrs[cur_pgmap_invlpg_count - 1] == addr)
		return;

	cur_pgmap_invlpg_addrs[cur_pgmap_invlpg_count++] = addr;
}

// Traverse the page table hierarchy between virtual addresses 'first' and
// 'last', inclusive. For each corresponding, present page table entry, call
// callback 'cb' with 'arg'. Possible callbacks are:
//     as_arch_collect_dirty_bits
//     as_arch_page_invalidate_cb,
//     as_arch_page_map_ro_cb.
//
// If 'create' is true, allocate any L2 page tables necessary to fulfill the
// request. This appears to be only the case when called through pgdir_walk by
// as_arch_putpage.
//
// Returns 0 on success, else an error value.
int
page_map_traverse(struct Pagemap *pgmap, const void *first, const void *last,
		  int create, page_map_traverse_cb cb, const void *arg)
{
	uint32_t l1_fidx, l1_lidx, l2_fidx, l2_lidx;
	unsigned int i, j;
	int r;

	if (last >= (const void *)ULIM)
		last = (const void *)(ULIM - PGSIZE);
	
	l1_fidx = ADDR_2_L1IDX((uint32_t)first);
	l1_lidx = ADDR_2_L1IDX((uint32_t)last);

	assert(l1_lidx < NL1PTENTRIES);

	for (i = l1_fidx; i <= l1_lidx; i++) {
		struct Pagemap *l2pm;
		ptent_t pte = pgmap->pm_ent[i];

		if (ARM_MMU_L1_TYPE(pte) != ARM_MMU_L1_TYPE_COARSE) {
			if (!create)
				continue;

			r = pmap_l2_zalloc(pgmap, (void **)&l2pm);
			if (r < 0)
				return (r);

			pgmap->pm_ent[i] = ARM_MMU_L1_TYPE_COARSE |
			    (uintptr_t)kva2pa(l2pm);
			cpufunc.cf_dcache_flush_invalidate_range(
			    &pgmap->pm_ent[i], sizeof(pgmap->pm_ent[0]));
		}

		l2pm = (struct Pagemap *)pa2kva(PTE_ADDR(pgmap->pm_ent[i]));

		l2_fidx = (i == l1_fidx) ? ADDR_2_L2IDX((uint32_t)first) : 0;
		l2_lidx = (i == l1_lidx) ? ADDR_2_L2IDX((uint32_t)last) :
		    NL2PTENTRIES - 1;

		for (j = l2_fidx; j <= l2_lidx; j++) {
			uintptr_t ent_va;

			pte = l2pm->pm_ent[j];
			if (ARM_MMU_L2_TYPE(pte) != ARM_MMU_L2_TYPE_SMALL) {
				if (!create)
					continue;
				// else, cb will set it up.
			}

			ent_va = (i << L1_PT_SHIFT) | (j << L2_PT_SHIFT);
			cb(arg, &l2pm->pm_ent[j], (void *)ent_va);
		}
	}

	return (0);
}

// Used exclusively by pkdir_walk below...
static void
pgdir_walk_cb(const void *arg, ptent_t *ptep, void *va)
{
	ptent_t **pte_store = (ptent_t **)arg;
	assert(ptep != NULL);
	*pte_store = ptep;
}

// Obtain the address of the single page table entry corresponding to virtual
// address 'va' and return in 'pte_store'. If not found, set '*pte_store' to
// NULL. 
// 
// If 'create' is true, allocate any L2 page tables necessary to fulfill the
// request.
//
// Returns 0 unless some crazy error occured.
int
pgdir_walk(struct Pagemap *pgmap, const void *va,
	   int create, ptent_t **pte_store)
{
	int r;

	assert(pte_store != NULL);
	*pte_store = NULL;

	r = page_map_traverse(pgmap, va, va, create, &pgdir_walk_cb, pte_store);
	if (r < 0)
		return (r);

	if (*pte_store != NULL && !create)
		assert(ARM_MMU_L2_TYPE(**pte_store) == ARM_MMU_L2_TYPE_SMALL);

	return (0);
}

// Search for the pte that maps the given virtual address 'va'. If found, store
// the pte in '*pte_store' and return the kva of the physical address that 'va'
// refers to.
//
// Returns the kva page address, else NULL if no valid entry is found.
static void *
page_lookup(struct Pagemap *pgmap, void *va, ptent_t **pte_store)
{
	if ((uintptr_t) va >= ULIM)
		panic("page_lookup: va %p over ULIM", va);

	ptent_t *ptep;
	int r = pgdir_walk(pgmap, va, 0, &ptep);
	if (r < 0)
		panic("pgdir_walk(%p, create=0) failed: %d", va, r);

	if (pte_store)
		*pte_store = ptep;

	if (ptep == NULL || ARM_MMU_L2_TYPE(*ptep) != ARM_MMU_L2_TYPE_SMALL)
		return (NULL);

	return (pa2kva(PTE_ADDR(*ptep)));
}

// Determine whether the user may access the virtual memory space starting at
// 'ptr' and spanning 'nbytes'. 'reqflags' specifies the requisite permissions,
// i.e. SEGMAP_{READ,WRITE,EXEC}. This will fault in all necessary pages being
// faulted in, permitting kernel access without page faults.
//
// 'alignbytes' specifies an alignment restriction on 'ptr', to ensure that
// accessing it would not cause an alignment fault. Note that ARM has some
// strange alignment behaviour.
//
// NB: It's up to us to load cur_thread's address space if it hasn't already
//     been.
//
// Returns 0 if access is permissible, else an error value.
int
check_user_access2(const void *ptr, uint64_t nbytes,
		   uint32_t reqflags, int alignbytes)
{
	int r;

	assert(cur_thread);
	if (!cur_as) {
		r = thread_load_as(cur_thread);
		if (r < 0)
			return (r);

		as_switch(cur_thread->th_as);
		assert(cur_as);
	}

	ptent_t pte_flags = ARM_MMU_L2_TYPE_SMALL;
	if (reqflags & SEGMAP_WRITE)
		pte_flags |= ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURW);
	else
		pte_flags |= ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURO);

	if (nbytes == 0)
		return (0);

	int aspf = 0;
	int overflow = 0;
	uintptr_t iptr = (uintptr_t)ptr;
	uintptr_t start = ROUNDDOWN(iptr, PGSIZE);
	uintptr_t end = ROUNDUP(safe_addptr(&overflow, iptr, nbytes), PGSIZE);

	if (iptr & (alignbytes - 1))
		return (-E_INVAL);

	if (end <= start || overflow)
		return (-E_INVAL);

	for (uintptr_t va = start; va < end; va += PGSIZE) {
		if (va >= ULIM)
			return (-E_INVAL);

		ptent_t *ptep;
		if (cur_as->as_pgmap &&
		    page_lookup(cur_as->as_pgmap, (void *)va, &ptep)) {
			pvp_t *pvp;

			r = pvp_get(cur_as->as_pgmap, (void *)va, &pvp, 0);
			assert(r == 0);

			// Adjust accordingly if the pte is really read/write
			// and we're checking for writable access. We must do
			// this here so that the kernel doesn't trap on a write.
			if ((*pvp & PVP_DIRTYEMU) &&
			    (reqflags & SEGMAP_WRITE) &&
			    (*ptep & ARM_MMU_L2_SMALL_AP_MASK) ==
			     ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURO)) {
				*pvp |= PVP_DIRTYBIT;
				*ptep &= ~ARM_MMU_L2_SMALL_AP_MASK;
				*ptep |= ARM_MMU_L2_SMALL_AP(
				    ARM_MMU_AP_KRWURW);
				cpufunc.cf_dcache_flush_invalidate_range(ptep,
				    sizeof(*ptep));
				pmap_queue_invlpg(cur_as->as_pgmap, (void *)va);
			}

			if ((*ptep & pte_flags) == pte_flags)
				continue;
		}

		// If the page isn't present in the page table, or the
		// permissions aren't appropriate, take a page fault.

		aspf = 1;
		r = as_pagefault(cur_as, (void *) va, reqflags);
		if (r < 0)
			return (r);
	}

	//Flush any stale TLB entries that might have arisen from as_pagefault()
	if (aspf)
		as_switch(cur_as);

	return (0);
}

// Switch address spaces by loading the given pagemap into TTBR.
// If pm is NULL, we're to use the kernel page table.
void
pmap_set_current(struct Pagemap *pm)
{
	physaddr_t pma = (pm == NULL) ? kva2pa(&kpagemap) : kva2pa(pm);
	int flush_tlb = 0;

	assert((pma & (ARM_MMU_L1_ALIGNMENT - 1)) == 0);

	if (cur_pgmap != pm || cur_pgmap_invlpg_count > pmap_invlpg_max) {
		flush_tlb = 1;
	} else {
		for (uint32_t i = 0; i < cur_pgmap_invlpg_count; i++)
			cpufunc.cf_tlb_flush_entry(cur_pgmap_invlpg_addrs[i]);
	}

	cur_pgmap = pm;
	cur_pgmap_invlpg_count = 0;

	if (flush_tlb) {
		cp15_ttbr_set(pma);
		cp15_tlb_flush();
	}
}

/*
 * Page table traversal callbacks
 */
 
// Query whether the page mapped to by 'va' is dirty (has been written) or not.
// If so, set the pi_dirty bit in its associated page_info structure and then
// clear the bit for the next modification.
//
// Since ARM does not have a hardware dirty bit, we need to emulate it by
// mapping R/W pages as R/O and catching write faults. In doing so, we have
// already set pi_dirty, so it's not necessary here.
void
as_arch_collect_dirty_bits(const void *arg, ptent_t *ptep, void *va)
{
	const struct Pagemap *pgmap = arg;
	struct page_info *pi;
	pvp_t *pvp;
	int r;

	// bail immediately if this isn't a valid entry (don't want to call
	// pa2kva yet, etc).
	if (ARM_MMU_L2_TYPE(*ptep) != ARM_MMU_L2_TYPE_SMALL)
		return;

	r = pvp_get(pgmap, va, &pvp, 0);
	assert(r == 0);

	if ((*pvp & PVP_DIRTYBIT) == 0)
		return;

	// could be no page_info if it's an mmaped device, for instance
	pi = page_to_pageinfo(pa2kva(PTE_ADDR(*ptep)));
	if (pi != NULL)
		pi->pi_dirty = 1;
	*pvp &= ~PVP_DIRTYBIT;
	*ptep &= ~ARM_MMU_L2_SMALL_AP_MASK;
	*ptep |= ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURO);
	cpufunc.cf_dcache_flush_invalidate_range(ptep, sizeof(*ptep));
	pmap_queue_invlpg(pgmap, va);
}

// Invalidate the page associated with 'ptep'.
void
as_arch_page_invalidate_cb(const void *arg, ptent_t *ptep, void *va)
{
	const struct Pagemap *pgmap = arg;
	pvp_t *pvp;
	int was_rw;

	was_rw = ((*ptep & ARM_MMU_L2_SMALL_AP_MASK) == ARM_MMU_L2_SMALL_AP(
	    ARM_MMU_AP_KRWURW));

	as_arch_collect_dirty_bits(arg, ptep, va);

	if (ARM_MMU_L2_TYPE(*ptep) == ARM_MMU_L2_TYPE_SMALL) {
		int r = pvp_get(pgmap, va, &pvp, 0);
		assert(r == 0);

		// sane? as_arch_collect_dirty_bits should clear dirty, make r/o
		assert((*ptep & ARM_MMU_L2_SMALL_AP_MASK) ==
		    ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURO));
		assert((*pvp & PVP_DIRTYBIT) == 0);
		if (was_rw)
			assert(*pvp & PVP_DIRTYEMU);

		// PVP_DIRTYEMU => what the mapping truly is, regardless of pte
		if (*pvp & PVP_DIRTYEMU) {
			pagetree_decpin_write(pa2kva(PTE_ADDR(*ptep)));
		} else {
			assert((*ptep & ARM_MMU_L2_SMALL_AP_MASK) ==
			    ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURO));
			pagetree_decpin_read(pa2kva(PTE_ADDR(*ptep)));
		}

		*ptep = ARM_MMU_L2_TYPE_INVALID;
		cpufunc.cf_dcache_flush_invalidate_range(ptep, sizeof(*ptep));
		pmap_queue_invlpg(pgmap, va);
		*pvp = 0;
	} else {
		int r = pvp_get(pgmap, va, &pvp, 0);
		if (r == 0)
			*pvp = 0;
		assert(*ptep == ARM_MMU_L2_TYPE_INVALID);
	}
}

// Mark the page associated with 'ptep' ready only.
void
as_arch_page_map_ro_cb(const void *arg, ptent_t *ptep, void *va)
{
	const struct Pagemap *pgmap = arg;
	pvp_t *pvp;
	int r;

	if (ARM_MMU_L2_TYPE(*ptep) == ARM_MMU_L2_TYPE_SMALL) {
		r = pvp_get(pgmap, va, &pvp, 0);
		assert(r == 0);

		// nothing to do if truly read only already
		if ((*pvp & PVP_DIRTYEMU) == 0) {
			assert((*ptep & ARM_MMU_L2_SMALL_AP_MASK) ==
			    ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURO));
			return;
		}

		pagetree_decpin_write(pa2kva(PTE_ADDR(*ptep)));
		pagetree_incpin_read(pa2kva(PTE_ADDR(*ptep)));

		*ptep &= ~ARM_MMU_L2_SMALL_AP_MASK;
		*ptep |= ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURO);
		cpufunc.cf_dcache_flush_invalidate_range(ptep, sizeof(*ptep));
		pmap_queue_invlpg(pgmap, va);

		// leave the emulated dirty bit set!
		*pvp &= ~PVP_DIRTYEMU;
	}
}

/*
 * Finally, the function that implaces page mappings to begin with!
 */

// Establish a page entry for the page associated with virtual address 'va'.
// Before doing so, invalidate any previous entry and record the dirty bit
// status.
//
// Since ARM's MMU lacks a dirty bit, we need to emulate it by initially
// setting R/W pages as R/O, then noting a write fault and changing the
// mapping correspondingly. 
int
as_arch_putpage(struct Pagemap *pgmap, void *va, void *pp, uint32_t flags)
{
	ptent_t *ptep;
	pvp_t *pvp;
	ptent_t cacheflags;

	assert(PGOFF(pp) == 0);

	int r = pgdir_walk(pgmap, va, 1, &ptep);
	if (r < 0)
		return (r);

	assert(ptep != NULL);

	as_arch_page_invalidate_cb(pgmap, ptep, va);

	// honour no-cache mappings
	cacheflags = ARM_MMU_L2_SMALL_BUFFERABLE | ARM_MMU_L2_SMALL_CACHEABLE;
	if (flags & SEGMAP_NOCACHE)
		cacheflags = 0;

	// all pages read only first to emulate dirty bit
	*ptep = kva2pa(pp) | ARM_MMU_L2_TYPE_SMALL | cacheflags |
	    ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURO);
	cpufunc.cf_dcache_flush_invalidate_range(ptep, sizeof(*ptep));
	pmap_queue_invlpg(pgmap, va);

	r = pvp_get(pgmap, va, &pvp, 1);
	if (r < 0)
		return (r);

	assert(*pvp == 0);

	if (flags & SEGMAP_WRITE) {
		*pvp = PVP_DIRTYEMU;
		pagetree_incpin_write(pp);
	} else {
		*pvp = 0;
		pagetree_incpin_read(pp);
	}

	return (0);
}

// Handle dirty bit emulation page faults. All data access faults come through
// here, so return 0 if handled, or non-0 if the fault is not due to dirty bit
// emulation.
int
arm_dirtyemu(struct Pagemap *pgmap, const void *fault_va)
{
	ptent_t *ptep;
	pvp_t *pvp;

	assert(pgmap != &kpagemap);

	int r = pgdir_walk(pgmap, fault_va, 0, &ptep);
	if (r < 0 || ptep == NULL)
		return (1);

	r = pvp_get(pgmap, fault_va, &pvp, 0);
	assert(r == 0);

	if ((*pvp & PVP_DIRTYEMU) == 0)
		return (1);

	assert((*pvp & PVP_DIRTYBIT) == 0);

	*pvp |= PVP_DIRTYBIT;
	*ptep &= ~ARM_MMU_L2_SMALL_AP_MASK;
       	*ptep |= ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_KRWURW);
	cpufunc.cf_dcache_flush_invalidate_range(ptep, sizeof(*ptep));
	pmap_queue_invlpg(pgmap, (void *)fault_va);

	return (0);
}
