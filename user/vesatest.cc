extern "C" {
#include <stdio.h>
#include <inc/syscall.h>
}

#include <inc/error.hh>

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

    for (uint32_t i = 0; i < 256; i++) {
	uint8_t buf[1024];
	memset(buf, i, sizeof(buf));
	error_check(sys_fb_set(sizeof(buf) * i, sizeof(buf), buf));
    }
} catch (std::exception &e) {
    printf("Caught exception: %s\n", e.what());
}
