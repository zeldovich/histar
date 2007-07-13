#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/net.h>

#include <linux/irqreturn.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <archcall.h>
#include <linuxsyscall.h>
#include "netd.h"

static irqreturn_t
netd_interrupt(int irq, void *dev_id)
{
    netd_user_interrupt();
    return IRQ_HANDLED;
}

static int
register_netd(void)
{
    int r = request_irq(LIND_NETD_IRQ, netd_interrupt, IRQF_DISABLED, "netd", 0);
    if (r < 0) {
	printk(KERN_ERR "unable to allocate netd interrupt\n");	
	return 1;
    }

    return 0;
}

static int
set_inet_taint(char *str)
{
    netd_user_set_taint(str);
    return 1;
}

late_initcall(register_netd);
__setup("inet_taint=", set_inet_taint);
