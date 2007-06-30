#include <kern/lib.h>

#include <dev/amba.h>
#include <dev/leonsercons.h>

#include <machine/ambapp.h>
#include <machine/leon.h>
#include <machine/leon3.h>

/*
 *  Types and structure used for AMBA Plug & Play bus scanning 
 */

struct amba_device_table {
    uint32_t devnr;	/* numbrer of devices on AHB or APB bus */
    uint32_t *addr[16];	/* addresses to the devices configuration tables */
};

struct amba_confarea_type {
    struct amba_device_table ahbmst;
    struct amba_device_table ahbslv;
    struct amba_device_table apbslv;
    uint32_t apbmst;
};

/* Structure containing address to devices found on the AMBA Plug&Play bus */
struct amba_confarea_type amba_conf;

uint32_t 
amba_find_apbslv_addr(uint32_t vendor, uint32_t device, uint32_t *irq)
{
    uint32_t conf, iobar;
    
    for (uint32_t i = 0; i < amba_conf.apbslv.devnr; i++) {
	conf = amba_get_confword(amba_conf.apbslv, i, 0);
	if ((amba_vendor(conf) == vendor)
	    && (amba_device(conf) == device)) {
	    if (irq)
		*irq = amba_irq(conf);
	    iobar = amba_apb_get_membar(amba_conf.apbslv, i);
	    return amba_iobar_start(amba_conf.apbmst, iobar);
	}
    }
    return 0;
}

uint32_t 
amba_find_next_apbslv_devices(uint32_t vendor, uint32_t device, 
			      struct amba_apb_device * dev, uint32_t nr)
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
    if (!LEON_BYPASS_LOAD_PA(cfg_area))
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
