#include <linux/init.h>
#include <linux/kernel.h>

#include <transport.h>
#include "rawsock.h"

#define MTU 1500

static int
register_rawsock(void)
{
    int r;
    struct transport trans;
    memset(&trans, 0, sizeof(trans));

    strncpy(trans.name, "eth0", sizeof(trans.name) - 1);
    trans.data_len = sizeof(struct rawsock_data);
    trans.mtu = MTU;
    trans.open = &rawsock_open;
    trans.tx = &rawsock_tx;
    trans.rx = &rawsock_rx;
    
    if (rawsock_mac(trans.mac) < 0) {
	printk(KERN_ERR "unable to get mac\n");
	return 1;
    }
    
    if ((r = register_transport(&trans)) < 0) {
	printk(KERN_ERR "unable to register rawsock\n");
	return 1;
    }
    return 0;
}

late_initcall(register_rawsock);
