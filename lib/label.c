#include <inc/lib.h>
#include <inc/syscall.h>

#include <inc/stdio.h>

int
label_get_cur(uint64_t ctemp, struct ulabel *l)
{
    int slot = sys_container_store_cur_thread(ctemp);
    if (slot < 0)
	return slot;

    int r = sys_obj_get_label(COBJ(ctemp, slot), l);
    sys_obj_unref(COBJ(ctemp, slot));

    return r;
}
