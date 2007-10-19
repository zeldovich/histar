#include <machine/x86.h>
#include <boot/code16gcc.h>

/*
 *  Enable A20:
 *   For fascinating historical reasons (related to the fact that
 *   the earliest 8086-based PCs could only address 1MB of physical memory
 *   and subsequent 80286-based PCs wanted to retain maximum compatibility),
 *   physical address line 20 is tied to low when the machine boots.
 *   Obviously this a bit of a drag for us, especially when trying to
 *   address memory above 1MB.  This code undoes this.

 * We don't bother frobbing the keyboard controller; all recent hardware
 * has the fast A20 gate on the chipset at 0x92.

 * "Fast A20 gate" -- new machines like laptops have no keyboard controller
 */
static void 
enable_a20_fast(void)
{
    uint8_t port_a;

    port_a = inb(0x92);     /* Configuration port A */
    port_a |=  0x02;        /* Enable A20 */
    port_a &= ~0x01;        /* Do not reset machine */
    outb(0x92, port_a);
}

void
smain(void)
{
    enable_a20_fast();
}
