#include <kern/lib.h>
#include <dev/amba.h>

#include <machine/ambapp.h>
#include <machine/leon.h>

void
amba_init(void)
{
    uint32_t *cfg_area;

    cfg_area = (uint32_t *)(LEON3_IO_AREA | LEON3_CONF_AREA);
    
    for (uint32_t i = 0; i < LEON3_AHB_MASTERS; i++) {
	if (LEON_BYPASS_LOAD_PA(cfg_area)) {
	    /* do something with config record..init some drivers? */
	}
	cfg_area += LEON3_AHB_CONF_WORDS;
    }
    
    cfg_area = (uint32_t *) (LEON3_IO_AREA | LEON3_CONF_AREA | 
			     LEON3_AHB_SLAVE_CONF_AREA);
    for (uint32_t i = 0; i < LEON3_AHB_SLAVES; i++) {
	if (LEON_BYPASS_LOAD_PA(cfg_area)) {
	    /* do something with config record..init some drivers? */
	}
	cfg_area += LEON3_AHB_CONF_WORDS;
    }
}
