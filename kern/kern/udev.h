#ifndef JOS_KERN_UDEV_H
#define JOS_KERN_UDEV_H

#include <machine/types.h>
#include <kern/thread.h>
#include <kern/intr.h>
#include <inc/queue.h>

struct udevice {
    void*		     arg;
    uint64_t		     key;

    uint64_t		     waiter_id;
    int64_t		     wait_gen;
    struct Thread_list	     wait_list;

    struct interrupt_handler ih;
    int			     intr_pend;

    void*		     iomap;
    uint64_t		     iomax;

    int     (*get_base)(void* a, uint64_t base, uint64_t* val);
    int     (*get_page)(void* a, uint64_t page_num, void** pp);

    LIST_ENTRY(udevice) link;
};

void	udev_register(struct udevice* udev);
int	udev_get_base(struct udevice* udev, uint64_t base, uint64_t* val);
int	udev_get_page(struct udevice* udev, uint64_t page_num, void** pp);
int64_t udev_thread_wait(struct udevice* udev, const struct Thread* t, 
			 uint64_t waiter, int64_t gen);
void	udev_thread_wakeup(struct udevice* udev);
int	udev_in_port(struct udevice* udev, uint64_t port, uint8_t width,
		     uint8_t* val, uint64_t n);
int	udev_out_port(struct udevice* udev, uint64_t port, uint8_t width,
		      uint8_t* val, uint64_t n);
void	udev_intr_enable(struct udevice* udev);

struct udevice* udev_get(uint64_t idx);

#endif
