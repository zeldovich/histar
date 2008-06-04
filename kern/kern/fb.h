#ifndef JOS_KERN_FB_H
#define JOS_KERN_FB_H

#include <machine/types.h>
#include <inc/fb.h>

struct fb_device {
    struct jos_fb_mode fb_mode;

    int (*fb_set) (void*, uint64_t, uint64_t, const uint8_t*);
    void *fb_arg;

    physaddr_t fb_base;
    uint64_t fb_size;
};

enum { fbdevs_max = 1 };
extern struct fb_device *fbdevs[fbdevs_max];
extern uint64_t fbdevs_num;

void fbdev_register(struct fb_device *fbdev);

#endif
