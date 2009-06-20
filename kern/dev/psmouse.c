#include <kern/intr.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <dev/kbdreg.h>
#include <dev/psmouse.h>
#include <inc/error.h>
#include <kern/mousedev.h>

enum { psmouse_debug = 0 };

// PS/2 Mouse packets are 3 bytes
static uint8_t psmouse_pktbuf[3];  // current pkt waiting to be read
static uint8_t psmouse_pktbuf_read; // flag to tell if data is fresh
static uint8_t psmouse_pkt[3]; // temp storage to build incoming pkt
static uint8_t psmouse_pos;

static struct mouse_device psmouse_dev;

static int
psmouse_read(uint8_t *buf, uint64_t nbytes, uint64_t off)
{
    // TODO ignore off?
    if (psmouse_pktbuf_read)
        // TODO What should we really do in this case?, need futex to wait on
        return 0;

    int r = nbytes < 3 ? nbytes : 3;
    memcpy(buf, psmouse_pktbuf, r);

    psmouse_pktbuf_read = 1;

    return r;
}

static int
psmouse_probe(void)
{
    return !psmouse_pktbuf_read;
}

static int
psmouse_proc_data(void *arg)
{
    //cprintf("Getting mouse int!\n");
    if ((inb(KBSTATP) & KBS_DIB) == 0)
        return -1;

    uint8_t data = inb(KBDATAP);
    psmouse_pkt[psmouse_pos++] = data;

    if (psmouse_pos >= 3) {
        if (psmouse_debug)
            cprintf("Got a mouse pkt 0x%02x%02x%02x\n", psmouse_pkt[0]
                                                      , psmouse_pkt[1]
                                                      , psmouse_pkt[2]);
        psmouse_pos = 0;
        // replace current pkt to be read with this one
        memcpy(psmouse_pktbuf, psmouse_pkt, 3);
        psmouse_pktbuf_read = 0;
    }

    return 0;
}

static void
psmouse_intr(void *arg)
{
    psmouse_proc_data(arg);
}

void
psmouse_init(void)
{
    // enable psaux
    cprintf("Initializing mouse\n");

    static struct interrupt_handler ih = {.ih_func = &psmouse_intr };
    irq_register(12, &ih);

    cprintf("PS/2 Aux IRQ 12 Handler installed\n");

    // TODO serious problems
    // 1) Need to check OBF flag before all writes (data or cmd)
    // 2) Need to check return codes after all writes

    // send aux enable
    outb(KBCMDP, KBC_AUXENABLE);

    outb(KBCMDP, KBC_RAMREAD);
    while ((inb(KBSTATP) & KBS_DIB) == 0);
    uint8_t cmdbyte = inb(KBDATAP);
    cprintf("Current PS/2 Command Byte 0x%.2x\n", cmdbyte);
    cprintf("PS/2 Aux IRQ 12 Enabling\n");
    cmdbyte = (cmdbyte | 0x02) & 0xEF;
    cprintf("Sending PS/2 Command Byte 0x%.2x\n", cmdbyte);

    // send int 12 enable command
    outb(KBCMDP, KBC_RAMWRITE);
    // get ack back how?
    outb(KBOUTP, cmdbyte);
    // get ack back how?
    outb(KBCMDP, KBC_AUXWRITE);
    outb(KBCMDP, KBC_AUXWRITE);
    outb(KBOUTP, 0xF4);
    // get ack back how?

    // start out being "read" until some real data comes in
    psmouse_pktbuf_read = 1;
    psmouse_dev.mouse_read = &psmouse_read;
    psmouse_dev.mouse_probe = &psmouse_probe;
    mousedev_register(&psmouse_dev);

    return;
    // TODO - it's be nice to get all this right

    while ((inb(KBSTATP) & KBS_DIB) == 0);

    uint8_t data = inb(KBDATAP);
    if (data == 0x00) {
        cprintf("PS/2 Mouse detected\n");
    } else {
        const char *msg[] = { "Success",
                              "Clock line stuck low",
                              "Clock line stuck high",
                              "Data line stuck low",
                              "Data line stuck high" };
        cprintf("PS/2 Mouse detection failed: (0x%.2x) %s\n", data, msg[data % 5]);
    }
}

