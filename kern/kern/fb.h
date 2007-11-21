#ifndef JOS_KERN_FB_H
#define JOS_KERN_FB_H

#include <inc/fb.h>

struct fb_dev {
    struct jos_fb_mode fb_mode;

    int (*fb_set) (void*, uint64_t, uint64_t, uint8_t*);
    void *fb_arg;
};

extern struct fb_dev *the_fb_dev;

#endif
