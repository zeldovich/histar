#include <machine/types.h>
#include <kern/lib.h>
#include <kern/mousedev.h>

struct mouse_device *mousedevs[mousedevs_max];
uint64_t mousedevs_num;

void
mousedev_register(struct mouse_device *mousedev)
{
    if (mousedevs_num >= mousedevs_max) {
	cprintf("mousedev_register: out of mousedev slots\n");
	return;
    }

    mousedevs[mousedevs_num++] = mousedev;
}
