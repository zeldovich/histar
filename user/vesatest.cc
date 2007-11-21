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

    uint8_t pixelbytes = fb_mode.bpp / 8;

    for (uint32_t i = 0; i < 1000000; i++) {
	uint8_t buf[pixelbytes];

	for (uint32_t j = 0; j < pixelbytes; j++)
	    buf[j] = random();

	uint32_t x0 = random() % fb_mode.xres;
	uint32_t y0 = random() % fb_mode.yres;
	uint32_t xd = random() % MIN(256, fb_mode.xres - x0);
	uint32_t yd = random() % MIN(256, fb_mode.yres - y0);

	for (uint32_t x = x0; x < x0 + xd; x++)
	    for (uint32_t y = y0; y < y0 + yd; y++)
		error_check(sys_fb_set(pixelbytes * (y * fb_mode.xres + x),
				       pixelbytes, buf));
    }
} catch (std::exception &e) {
    printf("Caught exception: %s\n", e.what());
}
