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
    bool_t		     intr_level;

    int     (*get_base)(void *a, uint64_t base, uint64_t *val);

    LIST_ENTRY(udevice) link;
};

void	udev_register(struct udevice* udev);
int	udev_get_base(struct udevice* udev, uint64_t base, uint64_t* val);
int64_t udev_thread_wait(struct udevice* udev, const struct Thread* t, 
			 uint64_t waiter, int64_t gen);
void	udev_thread_wakeup(struct udevice* udev);
int	udev_in_port(uint64_t key, uint64_t port, uint64_t* val);
int	udev_out_port(uint64_t key, uint64_t port, uint64_t val);
int	udev_intr_enable(struct udevice* udev);
int	udev_intr_disable(struct udevice* udev);

struct udevice* udev_get(uint64_t idx);

#endif
