#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/fb.h>
#include <dev/vesafb.h>
#include <inc/error.h>

struct vesafb_dev {
    struct vbe_control_info ctl_info;
    struct vbe_mode_info mode_info;
    uint32_t mode;

    struct fb_device fbdev;
};

static int
vesafb_set(void *arg, uint64_t offset, uint64_t nbytes, const uint8_t *buf)
{
    struct vesafb_dev *vfb = arg;

    if (offset + nbytes > PGSIZE * vfb->fbdev.fb_npages)
	return -E_INVAL;

    memcpy(pa2kva(vfb->fbdev.fb_base) + offset, buf, nbytes);
    return 0;
}

void
vesafb_init(struct vbe_control_info *ctl_info,
	    struct vbe_mode_info *mode_info,
	    uint32_t mode)
{
    static struct vesafb_dev vfb;
    memcpy(&vfb.ctl_info,  ctl_info,  sizeof(*ctl_info));
    memcpy(&vfb.mode_info, mode_info, sizeof(*mode_info));
    vfb.mode = mode;

    vfb.fbdev.fb_base   = vfb.mode_info.fb_physaddr;
    vfb.fbdev.fb_npages = ((uint32_t) vfb.ctl_info.memsize) * 65536 / PGSIZE;
    vfb.fbdev.fb_arg    = &vfb;
    vfb.fbdev.fb_set    = &vesafb_set;
    memcpy(&vfb.fbdev.fb_mode.vm, &vfb.mode_info, sizeof(vfb.fbdev.fb_mode.vm));

    fbdev_register(&vfb.fbdev);
}
