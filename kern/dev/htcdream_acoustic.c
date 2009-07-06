/*
 * This is an abuse of the framebuffer abstraction to provide user-level
 * access to the htc acoustic table, part of the shared memory segment that
 * libhtc_ril.so mmaps from /dev/htc-acoustic.
 */

#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/timer.h>
#include <kern/fb.h>
#include <kern/pageinfo.h>
#include <dev/htcdream_acoustic.h>
#include <inc/error.h>

static int
htcdream_acoustic_fb_set(void *arg, uint64_t offset, uint64_t nbytes, const uint8_t *buf)
{
    struct fb_device *fb = arg;

    if (offset + nbytes > PGSIZE * fb->fb_npages)
	return -E_INVAL;

    memcpy(pa2kva(fb->fb_base) + offset, buf, nbytes);
    return 0;
}

void htcdream_acoustic_init(uint32_t base, uint32_t len)
{
    cprintf("HTC DREAM Acoustic kernel stub: base @ pa 0x%08x (%u bytes)\n",
        base, len);

    // register our ``framebuffer'' device
    static struct fb_device fbdev_acoustic;
    memset(&fbdev_acoustic, 0, sizeof(fbdev_acoustic));
    fbdev_acoustic.fb_base   = base;
    fbdev_acoustic.fb_npages = (len + PGSIZE - 1) / PGSIZE; 
    fbdev_acoustic.fb_arg    = &fbdev_acoustic;
    fbdev_acoustic.fb_set    = &htcdream_acoustic_fb_set;
    fbdev_acoustic.fb_devmem = 1;
    fbdev_register(&fbdev_acoustic);
}
