#include <machine/x86.h>
#include <machine/boot.h>
#include <dev/vesa.h>
#include <boot/code16gcc.h>

enum { enable_vesa = 0 };

struct sysx_info sysx_info;

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

static void
detect_memory_e820(void)
{
    int count = 0;
    uint32_t next = 0;
    uint32_t size, id;
    uint8_t err;
    struct e820entry *desc = sysx_info.e820_map;
    
    do {
	size = sizeof(struct e820entry);
	
	/* Important: %edx is clobbered by some BIOSes,
	   so it must be either used for the error output
	   or explicitly marked clobbered. */
	__asm volatile("int $0x15; setc %0"
		       : "=d" (err), "+b" (next), "=a" (id), "+c" (size),
		       "=m" (*desc)
		       : "D" (desc), "d" (SMAP), "a" (0xe820));
	
	/* Some BIOSes stop returning SMAP in the middle of
	   the search loop.  We don't know exactly how the BIOS
	   screwed up the map at that point, we might have a
	   partial map, the full map, or complete garbage, so
	   just return failure. */
	if (id != SMAP) {
	    count = 0;
	    break;
	}
	
	if (err)
	    break;
	
	count++;
	desc++;
    } while (next && count < E820MAX);
    
    sysx_info.e820_nents = count;
}

static void
detect_memory_e801(void)
{
    uint16_t ax, bx, cx, dx;
    uint8_t err;

    bx = cx = dx = 0;
    ax = 0xe801;
    __asm volatile("stc; int $0x15; cli; setc %0"
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
}

static void
set_video(void)
{
    if (!enable_vesa)
	return;

    uint16_t ax, bx, cx, di;

    /* Get VBE control info */
    extern struct vbe_control_info vbe_control_info;
    vbe_control_info.sig[0] = 'V';
    vbe_control_info.sig[1] = 'B';
    vbe_control_info.sig[2] = 'E';
    vbe_control_info.sig[3] = '2';

    ax = 0x4f00;
    di = (uint16_t) (uintptr_t) &vbe_control_info;
    __asm volatile("int $0x10; cli"
		   : "+a" (ax) : "D" (di) : "memory");
    if (ax != 0x4f)
	return;

    /* Get VBE mode info */
    extern struct vbe_mode_info vbe_mode_info;
    uint16_t vbe_mode = 0x118;

    ax = 0x4f01;
    cx = vbe_mode;
    di = (uint16_t) (uintptr_t) &vbe_mode_info;
    __asm volatile("int $0x10; cli"
		   : "+a" (ax) : "c" (cx), "D" (di) : "memory");
    if (ax != 0x4f)
	return;

    /* Switch to VBE mode */
    ax = 0x4f02;
    bx = vbe_mode | VBE_MODE_FLAT_FB;
    __asm volatile("int $0x10; cli" : "+a" (ax) : "b" (bx) : "memory");
    if (ax != 0x4f)
	return;

    uint32_t ds = 0;
    __asm volatile("movw %%ds, %%ax" : "+a" (ds));

    sysx_info.vbe_control_info = (ds << 4) + (uint16_t) (uintptr_t) &vbe_control_info;
    sysx_info.vbe_mode_info = (ds << 4) + (uint16_t) (uintptr_t) &vbe_mode_info;
    sysx_info.vbe_mode = vbe_mode;
}

void
csetup(void)
{
    /*
     * Need to zero out sysx_info ourselves, since the compiler places
     * it in .bss, but no code exists to zero that out at runtime..
     */
    for (uint16_t i = 0; i < sizeof(sysx_info); i++)
	((uint8_t *) &sysx_info)[i] = 0;

    extern uint32_t cmd_line_ptr;
    enable_a20_fast();
    detect_memory_e820();
    detect_memory_e801();
    /* SYSLINUX copies the command line to somewhere in low memory
     * and saves a pointer in the Linux setup header.  We copy the
     * pointer over to our sysx_info.
     */
    sysx_info.cmdline = cmd_line_ptr;
    __asm volatile("movw $(0x0200 + '0'), %fs:(0x02)");
    set_video();
}
