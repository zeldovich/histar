#include <machine/pmap.h>
#include <kern/arch.h>
#include <inc/error.h>

int
check_user_access(const void *base __attribute__((unused)),
		  uint64_t nbytes __attribute__((unused)),
		  uint32_t flags __attribute__((unused)))
{
    return 0;
}

int
page_map_alloc(struct Pagemap **pm_store)
{
    *pm_store = 0;
    return -E_NO_MEM;
}

void
page_map_free(struct Pagemap *pgmap)
{
    if (pgmap)
	panic("page_map_free");
}

int
page_map_traverse(struct Pagemap *pgmap __attribute__((unused)), const void *first __attribute__((unused)), const void *last __attribute__((unused)),
		  int create __attribute__((unused)), page_map_traverse_cb cb __attribute__((unused)), const void *arg __attribute__((unused)))
{
    return -E_NO_MEM;
}

int
pgdir_walk(struct Pagemap *pgmap __attribute__((unused)), const void *va __attribute__((unused)),
	   int create __attribute__((unused)), uint64_t **pte_store __attribute__((unused)))
{
    return -E_NO_MEM;
}

void
pmap_tlb_invlpg(const void *va __attribute__((unused)))
{
}

void
pmap_set_current(struct Pagemap *pm __attribute__((unused)), int flush_tlb __attribute__((unused)))
{
    // hmm
}
