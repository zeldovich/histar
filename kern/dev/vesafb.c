#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/fb.h>
#include <dev/vesafb.h>
#include <inc/error.h>

struct vesafb_dev {
    struct vbe_control_info ctl_info;
    struct vbe_mode_info mode_info;
    uint32_t mode;

    uint8_t *fb_base;
    uint64_t fb_size;

    struct fb_dev fbdev;
};

static int
vesafb_set(void *arg, uint64_t offset, uint64_t nbytes, uint8_t *buf)
{
    struct vesafb_dev *vfb = arg;

    if (offset + nbytes > vfb->fb_size)
	return -E_INVAL;

    memcpy(vfb->fb_base + offset, buf, nbytes);
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

    vfb.fb_base = pa2kva(vfb.mode_info.fb_physaddr);
    vfb.fb_size = ((uint32_t) vfb.ctl_info.memsize) * 65536;

    vfb.fbdev.fb_arg = &vfb;
    vfb.fbdev.fb_set = &vesafb_set;
    memcpy(&vfb.fbdev.fb_mode.vm, &vfb.mode_info, sizeof(vfb.fbdev.fb_mode.vm));

    the_fb_dev = &vfb.fbdev;
}
