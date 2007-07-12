#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/if.h>
#include <linux/skbuff.h>

#include <transport.h>
#include "jif.h"

#define MTU 1500

static int
register_jif(void)
{
    int r, i, cnt;
    struct jif_list list[8];

    cnt = jif_list(list, 8);
    if (cnt < 0) {
	printk(KERN_ERR "unable to get jif list\n");
	return 1;
    }
    
    for (i = 0; i < cnt; i++) {
	struct transport trans;
	memset(&trans, 0, sizeof(trans));
	strncpy(trans.name, list[i].name, sizeof(trans.name) - 1);
	memcpy(trans.mac, list[i].mac, sizeof(trans.mac));
	trans.data_len = list[i].data_len;
	trans.mtu = MTU;
	trans.open = &jif_open;
	trans.tx = &jif_tx;
	trans.rx = &jif_rx;
	trans.irq_reset = &jif_irq_reset;
	
	if ((r = register_transport(&trans)) < 0) {
	    printk(KERN_ERR "unable to register rawsock\n");
	    return 1;
	}
    }
    
    return 0;
}

late_initcall(register_jif);
