#include <kern/lib.h>
#include <dev/greth.h>
#include <dev/grethreg.h>
#include <dev/ambapp.h>
#include <dev/amba.h>

static const char greth_mac[6] = { 0x00, 0x50, 0x56, 0xC0, 0x00, 0x11 };

void
greth_init(void)
{
    struct amba_apb_device dev;
    uint32_t r = amba_apbslv_device(VENDOR_GAISLER, GAISLER_ETHMAC, &dev, 0);
    if (!r)
	return;
}
