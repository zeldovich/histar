#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <inc/stdio.h>
#include <inc/assert.h>

#include <linux/fb.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>

static int
fb_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "fb");
    if (r < 0) {
	errno = ENOMEM;
	return -1;
    }

    fd->fd_dev_id = devfb.dev_id;
    fd->fd_omode = flags;
    return fd2num(fd);
}

static int
fb_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    switch (req) {
    case FBIOGET_CON2FBMAP: {
	struct fb_con2fbmap *c2m = va_arg(ap, struct fb_con2fbmap*);
	c2m->framebuffer = 0;
	return 0;
    }

    case FBIOPUT_VSCREENINFO: {
	/* potentially bad, but we have no way of changing VESA modes.. */
	return 0;
    }

    case FBIOGET_VSCREENINFO: {
	struct fb_var_screeninfo *v = va_arg(ap, struct fb_var_screeninfo*);
	memset(v, 0, sizeof(*v));

	struct jos_fb_mode fbmode;
	int r = sys_fb_get_mode(&fbmode);
	if (r < 0) {
	    __set_errno(EINVAL);
	    return -1;
	}

	v->xres = fbmode.vm.xres;
	v->yres = fbmode.vm.yres;
	v->bits_per_pixel = fbmode.vm.bpp;

	v->red.offset = fbmode.vm.fb_color[vbe_red].fieldpos;
	v->red.length = fbmode.vm.fb_color[vbe_red].masksize;
	v->green.offset = fbmode.vm.fb_color[vbe_green].fieldpos;
	v->green.length = fbmode.vm.fb_color[vbe_green].masksize;
	v->blue.offset = fbmode.vm.fb_color[vbe_blue].fieldpos;
	v->blue.length = fbmode.vm.fb_color[vbe_blue].masksize;
	v->transp.offset = fbmode.vm.fb_color[vbe_reserved].fieldpos;
	v->transp.length = fbmode.vm.fb_color[vbe_reserved].masksize;

	/* just like linux does it in vesafb.c */
	v->xres_virtual = fbmode.vm.xres;
	v->yres_virtual = fbmode.vm.yres;
	v->pixclock = 10000000 / v->xres * 1000 / v->yres;
	v->left_margin = (v->xres / 8) & 0xf8;
	v->hsync_len = (v->xres / 8) & 0xf8;

	if (v->bits_per_pixel == 15)
	    v->bits_per_pixel = 16;
	if (v->bits_per_pixel <= 8)
	    v->red.length = v->green.length = v->blue.length = v->bits_per_pixel;

	return 0;
    }

    case FBIOGET_FSCREENINFO: {
	struct fb_fix_screeninfo *f = va_arg(ap, struct fb_fix_screeninfo*);
	memset(f, 0, sizeof(*f));

	struct jos_fb_mode fbmode;
	int r = sys_fb_get_mode(&fbmode);
	if (r < 0) {
	    __set_errno(EINVAL);
	    return -1;
	}

	snprintf(&f->id[0], sizeof(f->id), "josfb");
	f->type = FB_TYPE_PACKED_PIXELS;
	f->line_length = fbmode.vm.bytes_per_scanline;
	f->visual = (fbmode.vm.bpp == 8) ? FB_VISUAL_STATIC_PSEUDOCOLOR
					 : FB_VISUAL_TRUECOLOR;
	return 0;
    }

    default:
	__set_errno(ENOSYS);
	return -1;
    }
}

static ssize_t
fb_write(struct Fd *fd, const void *buf, size_t len, off_t offset)
{
    int r = sys_fb_set(offset, len, buf);
    if (r < 0) {
	__set_errno(EINVAL);
	return -1;
    }

    return len;
}

struct Dev devfb = {
    .dev_id = 'F',
    .dev_name = "fb",
    .dev_open = fb_open,
    .dev_ioctl = fb_ioctl,
    .dev_write = fb_write,
};

