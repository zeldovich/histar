#include <linux/init.h>


static int console_chan_setup(char *str)
{
    cprintf("here!\n");
    return -1;
}
__setup("con", console_chan_setup);
