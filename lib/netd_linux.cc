extern "C" {
#include <inc/lib.h>
#include <inc/netdlinux.h>
#include <inc/stdio.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>

int
netd_linux_server_init(void (*handler)(struct netd_op_args *))
{
    return 0;
}
