#include <kern/udev.h>
#include <kern/lib.h>
#include <kern/arch.h>
#include <inc/error.h>
#include <inc/bitops.h>

enum { udevs_max = 32 };
static struct udevice* udevs[udevs_max];
static uint64_t	       udevs_num;

struct udevice*
udev_get(uint64_t idx)
{
    if (idx >= udevs_num)
	return 0;
    return udevs[idx];
}

static void
udev_intr(void* x)
{
    struct udevice *udev = x;
    if (udev->intr_pend)
	irq_arch_disable(udev->ih.ih_trapno);
    udev->intr_pend = 1;
    udev->wait_gen++;
    udev_thread_wakeup(udev);
}

int
udev_get_base(struct udevice* udev, uint64_t base, uint64_t* val)
{
    return udev->get_base(udev->arg, base, val);
}

int64_t
udev_thread_wait(struct udevice* udev, const struct Thread* t, 
		 uint64_t waiter, int64_t gen)
{
    if (!udev->ih.ih_trapno)
	irq_register(&udev->ih);

    if (waiter != udev->waiter_id) {
	udev->waiter_id = waiter;
	udev->wait_gen = 0;
	return -E_AGAIN;
    }

    if (gen != udev->wait_gen)
	return udev->wait_gen;

    thread_suspend(t, &udev->wait_list);
    return 0;
}

void
udev_thread_wakeup(struct udevice* udev)
{
    if (udev->wait_gen <= 0)
	udev->wait_gen = 1;
    
    while (!LIST_EMPTY(&udev->wait_list)) {
	struct Thread *t = LIST_FIRST(&udev->wait_list);
	thread_set_runnable(t);
    }
}

static int
check_port(struct udevice* udev, uint64_t port, uint8_t width)
{
    if (!udev->iomap || port + width > udev->iomax)
	return 0;
    for (uint8_t i = 0; i < width; i++)
	if (bit_get(udev->iomap, port + i) == 0)
	    return 0;
    return 1;
}

int
udev_in_port(struct udevice* udev, uint64_t port, uint8_t width, 
	     uint8_t* val, uint64_t n)
{
    if (check_port(udev, port, width))
	return arch_in_port(port, width, val, n);
    return -E_INVAL;
}

int
udev_out_port(struct udevice* udev, uint64_t port, uint8_t width, 
	      uint8_t* val, uint64_t n)
{
    if (check_port(udev, port, width))    
	return arch_out_port(port, width, val, n);
    return -E_INVAL;
}

void
udev_intr_enable(struct udevice* udev)
{
    irq_arch_enable(udev->ih.ih_trapno);
    udev->intr_pend = 0;
}

void
udev_register(struct udevice* udev)
{
    if (udevs_num >= udevs_max) {
	cprintf("udev_register: out of udev slots\n");
	return;
    }

    assert(udev->ih.ih_tbdp);
    udev->ih.ih_func = &udev_intr;
    udev->ih.ih_arg = udev;
    udev->ih.ih_user = 1;
    udevs[udevs_num++] = udev;
}
