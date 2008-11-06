#include <machine/mp.h>
#include <kern/arch.h>
#include <dev/picirq.h>
#include <dev/apic.h>

enum { use_ioapic = 1 };

/*
 * With the APIC a unique vector can be assigned to each
 * request to enable an interrupt. There are two reasons this
 * is a good idea:
 * 1) to prevent lost interrupts, no more than 2 interrupts
 *    should be assigned per block of 16 vectors See Intel Arch. 
 *    manual 3a, section 9.8.4.
 * 2) each input pin on the IOAPIC will receive a different
 *    vector regardless of whether the devices on that pin use
 *    the same IRQ as devices on another pin.
 *
 * The trap space is split into a kernel area and a user area.
 * All kernel traps have a higher priority than user traps.
 * the_schedtmr is the only driver that must use a kernel trap,
 * while all untrusted drivers must use user traps.  This allows 
 * the kernel to emulate cli for user-level drivers using 
 * APIC.TPR, but still preempt long running drivers.
 *
 * If there is no IOAPIC or devices do not support MSI we can't 
 * do this trick.
 */

static uint32_t kern_trapno = T_KERNDEV;
static uint32_t user_trapno = T_USERDEV;

void
irq_arch_enable(trapno_t trapno)
{
    /* Assume APIC code enables local interrupts */
    if (trapno >= T_LAPIC && trapno <= T_LAPIC_MAX)
	return;

    if (use_ioapic)
	mp_intrenable(trapno);
    else 
	pic_intrenable(trapno);
}

trapno_t
irq_arch_init(uint32_t irq, tbdp_t tbdp, bool_t user)
{
    trapno_t* x_trapno, r;

    if (irq > MAX_IRQ_PIC && irq <= MAX_IRQ_LAPIC)
	return APIC_TRAPNO(irq);

    if (!use_ioapic)
	return PIC_TRAPNO(irq);

    if (user) {
	if (user_trapno > T_USERDEV_MAX)
	    panic("out of user traps");
	x_trapno = &user_trapno;
    } else {
	if (kern_trapno > T_KERNDEV_MAX)
	    panic("out of kern traps");
	x_trapno = &kern_trapno;
    }

    r = mp_intrinit(irq, tbdp, *x_trapno);
    if (r == *x_trapno)
	*x_trapno += 8;

    return r;
}

void
irq_arch_disable(uint32_t trapno)
{
    if (!use_ioapic)
	panic("not supported without IOAPIC");
    if (trapno >= T_LAPIC && trapno <= T_LAPIC_MAX)
	panic("APIC trapno %u not supported", trapno);

    mp_intrdisable(trapno);
}

void
irq_arch_eoi(uint32_t trapno)
{
    if (APIC_IRQNO(trapno) == IRQ_SPURIOUS)
	return;
    if ((trapno >= T_LAPIC && trapno <= T_LAPIC_MAX) || use_ioapic)
	apic_eoi();
    else
	pic_eoi();
}

void
irq_arch_umask_enable(void)
{
    /*
     * priority = trapno / 16
     * The processor will service only those interrupts that 
     * have a priority higher than that specified in APIC.TPR.
     */
    apic_set_tpr((T_KERNDEV / 16) - 1);
}

void
irq_arch_umask_disable(void)
{
    apic_set_tpr(0);
}
