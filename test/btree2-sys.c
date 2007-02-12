#include <btree/sys.h>
#include <btree/error.h>

#include <malloc.h>
#include <assert.h>
#include <string.h>

#include <dmalloc.h>

static char dirty;

uint32_t read_cnt;
uint32_t write_cnt;

int
sys_alloc(size_t n, offset_t *off)
{
    void *a = malloc(n);
    if (!a) 
	return -E_NO_MEM;
    *off = (offset_t) a;

    dirty = 1;
    return 0;
}

int
sys_free(offset_t off)
{
    free((void *)off);
    return 0;
}

int
sys_flush(void)
{
    dirty = 0;

    //printf("read_cnt   %d\n", read_cnt);
    //printf("write_cnt  %d\n", write_cnt);

    read_cnt = 0;
    write_cnt = 0;
    
    return 0;
}

int
sys_clear(void)
{
    assert(!dirty);
    
    read_cnt = 0;
    write_cnt = 0;
    
    return 0;
}

void
sys_read(size_t n, offset_t off, void *buf)
{
    read_cnt++;
    char *ptr = (char *) off;
    memcpy(buf, ptr, n);
}

void
sys_write(size_t n, offset_t off, void *buf)
{
    dirty = 1;
    write_cnt++;
    char *ptr = (char *) off;
    memcpy(ptr, buf, n);
}
