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

static int
set_inet_taint(char *str)
{
    netd_user_set_taint(str);
    return 1;
}

__setup("inet_taint=", set_inet_taint);
