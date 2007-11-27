#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <dev/grata.h>
#include <dev/gratareg.h>
#include <dev/ambapp.h>
#include <dev/amba.h>
#include <inc/error.h>

int
grata_attach(struct amba_apb_device *dev)
{
    return -E_INVAL;
}
