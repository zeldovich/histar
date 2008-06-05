#include <kern/arch.h>
#include <kern/device.h>
#include <kern/kobj.h>
#include <inc/error.h>

void
device_swapin(struct Device *dv)
{
    LIST_INIT(&dv->dv_segmap_list);
}
