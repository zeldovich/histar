extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <inc/syscall.h>
}

#include <inc/error.hh>

enum { bufsize = 1024 };

int
main(int ac, char **av)
try
{
    printf("Starting VESA test..\n");

    struct jos_fb_mode fb_mode;
    error_check(sys_fb_get_mode(&fb_mode));
    printf("xres = %d\n", fb_mode.xres);
    printf("yres = %d\n", fb_mode.yres);
    printf("bpp  = %d\n", fb_mode.bpp);

    for (uint32_t i = 0; i < 1000000; i++) {
	uint8_t buf[3];

	buf[0] = random();
	buf[1] = random();
	buf[2] = random();

	uint32_t x0 = random() % (fb_mode.xres - 3);
	uint32_t y0 = random() % (fb_mode.yres - 3);
	uint32_t xd = random() % MIN(256, fb_mode.xres - 3 - x0);
	uint32_t yd = random() % MIN(256, fb_mode.yres - 3 - y0);

	for (uint32_t x = x0; x < x0 + xd; x++)
	    for (uint32_t y = y0; y < y0 + yd; y++)
		error_check(sys_fb_set(3 * (y * fb_mode.xres + x), 3, buf));
    }
} catch (std::exception &e) {
    printf("Caught exception: %s\n", e.what());
}
