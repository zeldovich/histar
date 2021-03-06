#include <kern/lib.h>

#include <dev/amba.h>
#include <dev/ambapp.h>

#include <dev/greth.h>
#include <dev/grata.h>

#include <machine/leon.h>
#include <machine/leon3.h>

/* AMBA peripheral device drivers */
static struct {
    uint32_t vendor;
    uint32_t device;
    int (*attach)(struct amba_apb_device *);
} apb_drivers[] = {
    { VENDOR_GAISLER, GAISLER_ETHMAC,  &greth_attach },
    { VENDOR_GAISLER, GAISLER_ATACTRL, &grata_attach },
};

/*
 *  Types and structure used for AMBA Plug & Play bus scanning 
 */

struct amba_device_table {
    uint32_t devnr;	/* numbrer of devices on AHB or APB bus */
    uint32_t *addr[16];	/* addresses to the devices configuration tables */
};

/* Structure containing address to devices found on the AMBA Plug&Play bus */
static struct {
    struct amba_device_table ahbmst;
    struct amba_device_table ahbslv;
    struct amba_device_table apbslv;
    uint32_t apbmst;
} amba_conf;

static void 
vendor_dev_string(uint32_t conf, char *venbuf, char *devbuf)
{
    uint32_t ven = amba_vendor(conf);
    uint32_t dev = amba_device(conf);
    const char *devstr;
    const char *venstr;

    sprintf(venbuf, "Unknown vendor %2x", ven);
    sprintf(devbuf, "Unknown device %2x", dev);

    venstr = vendor_id2str(ven);
    if (venstr)
	sprintf(venbuf, "%s", venstr);

    devstr = device_id2str(ven, dev);
    if (devstr)
	sprintf(devbuf, "%s", devstr);
}

void
amba_print(void)
{
    char venbuf[128];
    char devbuf[128];
    uint32_t conf;

    cprintf("AHB masters:\n");
    for (uint32_t i = 0; i < amba_conf.ahbmst.devnr; i++) {
	conf = amba_get_confword(amba_conf.ahbmst, i, 0);
	vendor_dev_string(conf, venbuf, devbuf);
	cprintf("%2d %-16s %-16s\n", i, venbuf, devbuf);
    }
    
    cprintf("AHB slaves:\n");
    for (uint32_t i = 0; i < amba_conf.ahbslv.devnr; i++) {
	conf = amba_get_confword(amba_conf.ahbslv, i, 0);
	vendor_dev_string(conf, venbuf, devbuf);
	cprintf("%2d %-16s %-16s\n", i, venbuf, devbuf);
    }

    cprintf("APB slaves:\n");
    for (uint32_t i = 0; i < amba_conf.apbslv.devnr; i++) {
	conf = amba_get_confword(amba_conf.apbslv, i, 0);
	vendor_dev_string(conf, venbuf, devbuf);
	cprintf("%2d %-16s %-16s\n", i, venbuf, devbuf);
    }
}

void
amba_attach(void)
{
    for (uint32_t i = 0; i < sizeof(apb_drivers) / sizeof(apb_drivers[0]); i++) {
	struct amba_apb_device dev;
	int r = amba_apbslv_device(apb_drivers[i].vendor,
				   apb_drivers[i].device,
				   &dev, 0);
	if (r <= 0)
	    continue;

	r = apb_drivers[i].attach(&dev);
	if (r < 0)
	    cprintf("amba_attach: cannot attach 0x%x.0x%x\n",
		    apb_drivers[i].vendor, apb_drivers[i].device);
    }
}

uint32_t 
amba_ahbslv_device(uint32_t vendor, uint32_t device, 
		   struct amba_ahb_device * dev, uint32_t nr)
{
    uint32_t start, stop, conf, iobar, j = 0;
    for (uint32_t i = 0; i < amba_conf.ahbslv.devnr; i++) {
	conf = amba_get_confword(amba_conf.ahbslv, i, 0);
	if ((amba_vendor(conf) == vendor) && (amba_device(conf) == device)) {
	    if (j == nr) {
		for (uint32_t k = 0; k < 4; k++) {
		    iobar = amba_ahb_get_membar(amba_conf.ahbslv, i, k);
		    start = amba_membar_start(iobar);
		    stop = amba_membar_stop(iobar);
		    if (amba_membar_type(iobar) == AMBA_TYPE_AHBIO) {
			start = amba_type_ahbio_addr(start);
			stop = amba_type_ahbio_addr(stop);
		    }
		    dev->start[k] = start;
		    dev->stop[k] = stop;
		}
		dev->irq = amba_irq(conf);
		return 1;
	    }
	    j++;
	}
    }
    return 0;
}

uint32_t 
amba_apbslv_device(uint32_t vendor, uint32_t device, 
		   struct amba_apb_device *dev, uint32_t nr)
{
    uint32_t conf, iobar, j = 0;
    
    for (uint32_t i = 0; i < amba_conf.apbslv.devnr; i++) {
	conf = amba_get_confword(amba_conf.apbslv, i, 0);
	if ((amba_vendor(conf) == vendor) && (amba_device(conf) == device)) {
	    if (j == nr) {
		dev[0].irq = amba_irq(conf);
		iobar = amba_apb_get_membar(amba_conf.apbslv, i);
		dev[0].start = amba_iobar_start(amba_conf.apbmst, iobar);
		return 1;
	    }
	    j++;
	}
    }
    return 0;
}

static int
amba_insert_device(struct amba_device_table *tab, uint32_t *cfg_area)
{
    if (!lda_bypass((physaddr_t) cfg_area))
	return 0;

    tab->addr[tab->devnr] = cfg_area;
    tab->devnr++;

    return 1;
}

/*
 * Used to scan system bus. Probes for AHB masters, AHB slaves and 
 * APB slaves. Addresses to configuration areas are stored in
 * amba_conf.
 */
void amba_init(void)
{
    uint32_t *cfg_area;
    uint32_t mbar, conf;
    
    memset(&amba_conf, 0, sizeof(amba_conf));
    cfg_area = (uint32_t *)(LEON3_IO_AREA | LEON3_CONF_AREA);

    for (uint32_t i = 0; i < LEON3_AHB_MASTERS; i++) {
	amba_insert_device(&amba_conf.ahbmst, cfg_area);
	cfg_area += LEON3_AHB_CONF_WORDS;
    }
    
    cfg_area = (uint32_t *)(LEON3_IO_AREA | LEON3_CONF_AREA |
			    LEON3_AHB_SLAVE_CONF_AREA);
    for (uint32_t i = 0; i < LEON3_AHB_SLAVES; i++) {
	amba_insert_device(&amba_conf.ahbslv, cfg_area);
	cfg_area += LEON3_AHB_CONF_WORDS;
    }
    
    for (uint32_t i = 0; i < amba_conf.ahbslv.devnr; i++) {
	conf = amba_get_confword(amba_conf.ahbslv, i, 0);
	mbar = amba_ahb_get_membar(amba_conf.ahbslv, i, 0);
	if ((amba_vendor(conf) == VENDOR_GAISLER) &&
	    (amba_device(conf) == GAISLER_APBMST)) {
	    amba_conf.apbmst = amba_membar_start(mbar);
	    cfg_area = (uint32_t *)(amba_conf.apbmst | LEON3_CONF_AREA);
	    for (uint32_t j = amba_conf.apbslv.devnr; 
		 j < LEON3_APB_SLAVES; j++) {
		amba_insert_device(&amba_conf.apbslv, cfg_area);
		cfg_area += LEON3_APB_CONF_WORDS;
	    }
	}
    }
}
