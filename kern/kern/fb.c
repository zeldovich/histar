#include <machine/types.h>
#include <kern/lib.h>
#include <kern/fb.h>

struct fb_device *fbdevs[fbdevs_max];
uint64_t fbdevs_num;

void
fbdev_register(struct fb_device *fbdev)
{
    if (fbdevs_num >= fbdevs_max) {
	cprintf("fbdev_register: out of fbdev slots\n");
	return;
    }

    fbdevs[fbdevs_num++] = fbdev;
}
