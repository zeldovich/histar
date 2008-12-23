#include <machine/nacl.h>

#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/pageinfo.h>
#include <kern/embedbin.h>
#include <inc/setjmp.h>
#include <inc/error.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

static int mem_fd;
static void *mem_base;
static uint64_t mem_bytes;
struct page_info *page_infos;

struct embed_bin embed_bins[] = { { 0, 0, 0 }, { 0, 0, 0 } };

void *
pa2kva(physaddr_t pa)
{
    return mem_base + pa;
}

physaddr_t
kva2pa(void *kva)
{
    physaddr_t addr = (physaddr_t) kva;
    physaddr_t base = (physaddr_t) mem_base;
    assert(addr >= base && addr < base + mem_bytes);
    return addr - base;
}

void
nacl_mem_init(const char *memfn, const char *binfn)
{
    void *free_end;
    uint64_t bytes;
    struct stat buf;

    if ((mem_fd = open(memfn, O_RDWR)) < 0)
	panic("open %s failed", memfn);
    
    if (fstat(mem_fd, &buf) < 0)
	panic("fstat failed");
    
    bytes = ROUNDDOWN(buf.st_size, PGSIZE);
    cprintf("%"PRIu64" bytes physical memory\n", bytes);

    mem_bytes = bytes;
    global_npages = bytes / PGSIZE;

    mem_base = mmap(0, mem_bytes, PROT_READ | PROT_WRITE, 
		    MAP_SHARED, mem_fd, 0);
    if (mem_base == MAP_FAILED)
	panic("mmap failed");
    assert(mem_base > (void *)ULIM);

    mem_base = ROUNDUP(mem_base, PGSIZE);
    free_end = mem_base + bytes;

    uint64_t pilen = (mem_bytes / PGSIZE) * sizeof(*page_infos);
    page_infos = malloc(pilen);
    memset(page_infos, 0, pilen);

    /* copy a binary into the end of memory */
    if (binfn) {
	int fd, r;
	void *b;

	if ((fd = open(binfn, O_RDONLY)) < 0)
	    panic("open %s failed", binfn);
	if (fstat(fd, &buf) < 0)
	    panic("fstat failed");
	if ((uint64_t)buf.st_size >= 2 * bytes)
	    panic("not enough mem for %s", binfn);

	b = ROUNDDOWN(mem_base + mem_bytes - buf.st_size, PGSIZE);
	embed_bins[0].name = "init";
	embed_bins[0].buf = b;
	embed_bins[0].size = buf.st_size;
	free_end = b;

	while ((r = read(fd, b, buf.st_size)) >= 0 && buf.st_size) {
	    b += r;
	    buf.st_size -= r;
	}
	if (r < 0)
	    panic("read failed");
    }

    page_alloc_init();
    for (void *p = mem_base; p < free_end; p += PGSIZE)
	page_free(p);
}

int
nacl_mmap(void *va, void *pp, int len, int prot)
{
    physaddr_t pa = kva2pa(pp);
    void *v = mmap(va, len, prot, MAP_SHARED | MAP_FIXED, mem_fd, pa);
    if (v == MAP_FAILED) {
	printf("nacl_mmap: failed for va %p pa %p\n", va, pp);
	return -E_NO_MEM;
    }
    assert(v == va);
    return 0;
}
