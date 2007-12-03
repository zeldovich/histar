#include <inc/lib.h>
#include <inc/fs.h>
#include <inc/types.h>
#include <inc/lfsimpl.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>

#include <linuxsyscall.h>
#include <archcall.h>
#include <os-jos64/kernelcall.h>

static void
lfs_handler(uint64_t arg, uint64_t x)
{
    int fd, r;
    struct lfs_op_args *b = (struct lfs_op_args *)arg;
    struct lfs_op_args stack_a;
    
    /* XXX create a copy because b doesn't point to any Linux
     * 'physical memory'..or we could change the address checking
     */
    memcpy(&stack_a, b, sizeof(stack_a));
    
    switch(stack_a.op_type) {
    case lfs_op_read:
	fd = linux_open(stack_a.pn, O_RDONLY, 0);
	if (fd < 0) {
	    stack_a.rval = fd;
	    return;
	}
	r = linux_lseek(fd, stack_a.offset, SEEK_SET);
	if (r != stack_a.offset) {
	    linux_close(fd);
	    arch_printf("lfs_handler: sys_lseek error: %d\n", r);
	    stack_a.rval = -1;
	    break;
	}
	stack_a.rval = linux_read(fd, stack_a.buf, stack_a.bufcnt);
	linux_close(fd);
	break;
    default:
	arch_printf("lfs_handler: unimplemented op %d\n", stack_a.op_type);
	stack_a.rval = -1;
	break;
    }

    memcpy(b, &stack_a, sizeof(*b));
}

static void
lfs_handler_stub(struct lfs_op_args *a)
{
    assert(kernel_call(lfs_handler, (uintptr_t) a, 0) == 0);
}

int
lfs_init(void)
{
    int r, i;
    struct cobj_ref gate;

    r = linux_mount("proc", "/proc", "proc", 0, 0);
    if (r < 0) {
	printk("lfs_init: cannot mount /proc\n");
	return r;
    }

    r = lfs_server_init(&lfs_handler_stub, &gate);
    if (r < 0) {
	printk("lfs_init: cannot init server\n");
	return r;
    }

    struct fs_inode netd;
    r = fs_namei("/netd", &netd);
    if (r < 0) {
	printk("lfs_init: cannot lookup /netd\n");
	return r;
    }

    r = symlink("proc/net/pnp", "/netd/resolv.conf");
    if (r < 0) {
	printk("lfs_init: cannot symlink resolv.conf");
	return r;
    }

    struct fs_inode proc;
    r = fs_mkdir(netd, "proc", &proc, 0);
    if (r < 0) {
	printk("lfs_init: cannot create proc dir\n");
	return r;
    }

    struct fs_inode net;
    r = fs_mkdir(proc, "net", &net, 0);
    if (r < 0) {
	printk("lfs_init: cannot create proc/net dir\n");
	return r;
    }

    const char *devs[] = { "arp", "dev", "pnp", "route", "tcp", "udp" };
    for (i = 0; i < sizeof(devs) / sizeof(devs[0]); i++) {
	char fullpn[64];
	sprintf(&fullpn[0], "/proc/net/%s", devs[i]);

	r = lfs_create(net, devs[i], fullpn, gate);
	if (r < 0) {
	    printk("lfs_init: cannot create %s\n", fullpn);
	    return r;
	}
    }

    return 0;
}
