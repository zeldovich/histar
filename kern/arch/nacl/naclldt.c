#include <machine/nacl.h>
#include <machine/error.h>
#include <machine/x86.h>
#include <kern/arch.h>

#include <asm/ldt.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

uint16_t kern_cs, kern_ds;
uint16_t user_cs, user_ds;

extern int modify_ldt(int func, void* ptr, unsigned long bytecount);

static int
get_ldt_slot(void)
{
    int slot = -1;
    int size = sizeof(uint64_t) * LDT_ENTRIES;
    uint64_t *entry = malloc(size);
    errno_check(modify_ldt(0, entry, size));

    for (int i = 0; i < LDT_ENTRIES; i++) {
	if (!(entry[i] & SEG_P)) {
	    slot = i;
	    break;
	}
    }

    free(entry);
    if (slot < 0)
	eprint("no free LDT slots");
    return slot;
}

static uint16_t
new_ldt_entry(int read_exec_only, uint32_t base_addr, uint32_t pglimit)
{
    struct user_desc ud;
    int slot;

    slot = get_ldt_slot();
    ud.entry_number = slot;
    ud.read_exec_only = read_exec_only;
    ud.seg_32bit = 1;
    ud.seg_not_present = 0;
    ud.useable = 1;
    ud.base_addr = base_addr;
    ud.limit = pglimit;
    ud.limit_in_pages = 1;
    errno_check(modify_ldt(1, &ud, sizeof(ud)));
    return ud.entry_number << 3 | 0x7;
}

void
nacl_seg_init(void)
{
    kern_cs = read_cs();
    kern_ds = read_ds();

    user_cs = new_ldt_entry(1, 0, UKSYSCALL / PGSIZE);
    //user_ds = new_ldt_entry(0, 0, (KBASE / PGSIZE) - 1);
}
