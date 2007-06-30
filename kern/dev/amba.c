#include <kern/lib.h>

#include <dev/amba.h>
#include <dev/leonsercons.h>

#include <machine/ambapp.h>
#include <machine/leon.h>

void
amba_init(void)
{
    uint32_t *cfg_area;

    cfg_area = (uint32_t *)(LEON3_IO_AREA | LEON3_CONF_AREA);
    
    for (uint32_t i = 0; i < LEON3_AHB_MASTERS; i++) {
	uint32_t conf = LEON_BYPASS_LOAD_PA(cfg_area);
	if (conf) {
	    /* XXX */
	}
	cfg_area += LEON3_AHB_CONF_WORDS;
    }
    
    cfg_area = (uint32_t *) (LEON3_IO_AREA | LEON3_CONF_AREA | 
			     LEON3_AHB_SLAVE_CONF_AREA);
    for (uint32_t i = 0; i < LEON3_AHB_SLAVES; i++) {
	uint32_t conf = LEON_BYPASS_LOAD_PA(cfg_area);
	if (conf) {
	    if (AMBA_CONF_VID(conf) == VENDOR_GAISLER &&
		AMBA_CONF_DID(conf) == GAISLER_APBMST) {
		uint32_t mbar = LEON_BYPASS_LOAD_PA(cfg_area + 4);
		uint32_t apb_start = amba_membar_start(mbar);
		
		uint32_t *apb_cfg = (uint32_t *) (apb_start | LEON3_CONF_AREA);
		for(uint32_t j = 0; j < LEON3_APB_SLAVES; j++) {
		    conf = LEON_BYPASS_LOAD_PA(apb_cfg);
		    if (AMBA_CONF_VID(conf) == VENDOR_GAISLER &&
			AMBA_CONF_DID(conf) == GAISLER_APBUART) {
			/* we have the serial line... */
			uint32_t iobar = LEON_BYPASS_LOAD_PA(apb_cfg + 1);
			uint32_t b = amba_iobar_start(apb_start, iobar);
			if (b)
			    leonsercons_init(conf, b);
		    }
		    apb_cfg += LEON3_APB_CONF_WORDS;
		}
	    }
	}
	cfg_area += LEON3_AHB_CONF_WORDS;
    }
}
