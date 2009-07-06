/*
 * This is an abuse of the framebuffer abstraction to provide user-level
 * access to the shared memory region and notification interrupt registers.
 */

#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/timer.h>
#include <kern/fb.h>
#include <kern/pageinfo.h>
#include <dev/msm_smd.h>
#include <inc/error.h>

static int
msm_smd_fb_set(void *arg, uint64_t offset, uint64_t nbytes, const uint8_t *buf)
{
    struct fb_device *fb = arg;

    if (offset + nbytes > PGSIZE * fb->fb_npages)
	return -E_INVAL;

    memcpy(pa2kva(fb->fb_base) + offset, buf, nbytes);
    return 0;
}

static void
msm_smd_intr(void *arg)
{
	/* do nothing and hope userland daemon is sys_irq_wait'ing. */
}

void msm_smd_init(uint32_t smd_base, uint32_t smd_len, uint32_t notify_base,
    uint32_t smd_irq, uint32_t smsm_irq)
{
    static struct interrupt_handler msm_smd_ih = {
        .ih_func = &msm_smd_intr
    };

    /* there's no real handler - this is up to userlevel */
    irq_register(smd_irq, &msm_smd_ih);
    irq_register(smsm_irq, &msm_smd_ih);

    cprintf("MSM SMD kernel stub: smd_base @ pa 0x%08x (%d bytes), "
        "notify_base @ pa 0x%08x, smd_irq %d, smsm_irq %d\n", smd_base, smd_len,
        notify_base, smd_irq, smsm_irq);

    // register our ``framebuffer'' devices
    static struct fb_device fbdev_smd;
    memset(&fbdev_smd, 0, sizeof(fbdev_smd));
    fbdev_smd.fb_base   = smd_base;
    fbdev_smd.fb_npages = (smd_len + PGSIZE - 1) / PGSIZE; 
    fbdev_smd.fb_arg    = &fbdev_smd;
    fbdev_smd.fb_set    = &msm_smd_fb_set;
    fbdev_smd.fb_devmem = 1;
    fbdev_register(&fbdev_smd);

    static struct fb_device fbdev_notify;
    memset(&fbdev_notify, 0, sizeof(fbdev_notify));
    fbdev_notify.fb_base   = notify_base;
    fbdev_notify.fb_npages = 1;
    fbdev_notify.fb_arg    = &fbdev_notify;
    fbdev_notify.fb_set    = &msm_smd_fb_set;
    fbdev_notify.fb_devmem = 1;
    fbdev_register(&fbdev_notify);
}
