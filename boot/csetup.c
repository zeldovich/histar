#include <machine/x86.h>
#include <machine/boot.h>
#include <boot/code16gcc.h>

extern struct sysx_info sysx_info;

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

static int 
detect_memory_e801(void)
{
    uint16_t ax, bx, cx, dx;
    uint8_t err;

    bx = cx = dx = 0;
    ax = 0xe801;
    __asm("stc; int $0x15; cli; setc %0"
	: "=m" (err), "+a" (ax), "+b" (bx), "+c" (cx), "+d" (dx));
    
    if (err || cx > 15*1024) {
	cx = 0;
	dx = 0;
    }
    
    /* This ignores memory above 16MB if we have a memory hole
       there.  If someone actually finds a machine with a memory
       hole at 16MB and no support for 0E820h they should probably
       generate a fake e820 map. */
    sysx_info.extmem_kb = (cx == 15*1024) ? (dx << 6)+cx : cx;
    return 0;
}

static void
set_video(void)
{
    /* XXX */
}

void
csetup(void)
{
    enable_a20_fast();
    detect_memory_e801();
    __asm ("movw $(0x0200 + '0'), %es:(0x02)");
    set_video();
}
