#include <kern/console.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/prof.h>
#include <dev/cgacons.h>
#include <dev/kbdreg.h>

/***** Text-mode CGA/VGA display output *****/

static unsigned addr_6845;
static uint16_t *crt_buf;
static uint16_t crt_pos;

#if CRT_SAVEROWS > 0
static uint16_t crtsave_buf[CRT_SAVEROWS * CRT_COLS];
static uint16_t crtsave_pos;
static int16_t crtsave_backscroll;
static uint16_t crtsave_size;
#endif

static void
cga_init(void)
{
    volatile uint16_t *cp;
    uint16_t was;
    unsigned pos;

    cp = (uint16_t *) (KERNBASE + CGA_BUF);
    was = *cp;
    *cp = (uint16_t) 0xA55A;
    if (*cp != 0xA55A) {
	cp = (uint16_t *) (KERNBASE + MONO_BUF);
	addr_6845 = MONO_BASE;
    } else {
	*cp = was;
	addr_6845 = CGA_BASE;
    }

    /* Extract cursor location */
    outb(addr_6845, 14);
    pos = inb(addr_6845 + 1) << 8;
    outb(addr_6845, 15);
    pos |= inb(addr_6845 + 1);

    crt_buf = (uint16_t *) cp;
    crt_pos = pos;
}


#if CRT_SAVEROWS > 0
// Copy one screen's worth of data to or from the save buffer,
// starting at line 'first_line'.
static void
cga_savebuf_copy(int first_line, bool_t to_screen)
{
    uint16_t *pos;
    uint16_t *end;
    uint16_t *trueend;

    // Calculate the beginning & end of the save buffer area.
    pos = crtsave_buf + (first_line % CRT_SAVEROWS) * CRT_COLS;
    end = pos + CRT_ROWS * CRT_COLS;
    // Check for wraparound.
    trueend = MIN(end, crtsave_buf + CRT_SAVEROWS * CRT_COLS);

    // Copy the initial portion.
    if (to_screen)
	memcpy(crt_buf, pos, (trueend - pos) * sizeof(uint16_t));
    else
	memcpy(pos, crt_buf, (trueend - pos) * sizeof(uint16_t));

    // If there was wraparound, copy the second part of the screen.
    if (end == trueend)
	/* do nothing */ ;
    else if (to_screen)
	memcpy(crt_buf + (trueend - pos), crtsave_buf,
	       (end - trueend) * sizeof(uint16_t));
    else
	memcpy(crtsave_buf, crt_buf + (trueend - pos),
	       (end - trueend) * sizeof(uint16_t));
}
#endif


static void
cga_putc(void *arg, int c)
{
#if CRT_SAVEROWS > 0
    // unscroll if necessary
    if (crtsave_backscroll > 0) {
	cga_savebuf_copy(crtsave_pos + crtsave_size, 1);
	crtsave_backscroll = 0;
    }
#endif

    // if no attribute given, then use black on white
    if (!(c & ~0xFF))
	c |= 0x0700;

    switch (c & 0xff) {
    case '\b':
	if (crt_pos > 0) {
	    crt_pos--;
	    crt_buf[crt_pos] = (c & ~0xff) | ' ';
	}
	break;
    case '\n':
	crt_pos += CRT_COLS;
	/* fallthru */
    case '\r':
	crt_pos -= (crt_pos % CRT_COLS);
	break;
    case '\t':
	cga_putc(arg, ' ');
	cga_putc(arg, ' ');
	cga_putc(arg, ' ');
	cga_putc(arg, ' ');
	cga_putc(arg, ' ');
	break;
    default:
	crt_buf[crt_pos++] = c;	/* write the character */
	break;
    }

    /* scroll if necessary */
    if (crt_pos >= CRT_SIZE) {
	int i;

#if CRT_SAVEROWS > 0
	// Save the scrolled-back row
	if (crtsave_size == CRT_SAVEROWS - CRT_ROWS)
	    crtsave_pos = (crtsave_pos + 1) % CRT_SAVEROWS;
	else
	    crtsave_size++;
	memcpy(crtsave_buf +
	       ((crtsave_pos + crtsave_size - 1) % CRT_SAVEROWS) * CRT_COLS,
	       crt_buf, CRT_COLS * sizeof(uint16_t));
#endif

	memcpy(crt_buf, crt_buf + CRT_COLS,
	       (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
	for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
	    crt_buf[i] = 0x0700 | ' ';
	crt_pos -= CRT_COLS;
    }

    /* move that little blinky thing */
    outb(addr_6845, 14);
    outb(addr_6845 + 1, crt_pos >> 8);
    outb(addr_6845, 15);
    outb(addr_6845 + 1, crt_pos);
}

#if CRT_SAVEROWS > 0
static void
cga_scroll(int delta)
{
    int new_backscroll = MIN(crtsave_backscroll - delta, crtsave_size);
    new_backscroll = MAX(new_backscroll, 0);

    if (new_backscroll == crtsave_backscroll)
	return;
    if (crtsave_backscroll == 0)
	// save current screen
	cga_savebuf_copy(crtsave_pos + crtsave_size, 0);

    crtsave_backscroll = new_backscroll;
    cga_savebuf_copy(crtsave_pos + crtsave_size - crtsave_backscroll, 1);
}
#endif

/***** Keyboard input code *****/

#define NO		0

#define SHIFT		(1<<0)
#define CTL		(1<<1)
#define ALT		(1<<2)

#define CAPSLOCK	(1<<3)
#define NUMLOCK		(1<<4)
#define SCROLLLOCK	(1<<5)

#define E0ESC		(1<<6)

static uint8_t shiftcode[256] = {
    [0x1D] CTL,
    [0x2A] SHIFT,
    [0x36] SHIFT,
    [0x38] ALT,
    [0x9D] CTL,
    [0xB8] ALT
};

static uint8_t togglecode[256] = {
    [0x3A] CAPSLOCK,
    [0x45] NUMLOCK,
    [0x46] SCROLLLOCK
};

static uint8_t normalmap[256] = {
    NO, 0x1B, '1', '2', '3', '4', '5', '6',	// 0x00
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',	// 0x10
    'o', 'p', '[', ']', '\n', NO, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	// 0x20
    '\'', '`', NO, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', NO, '*',	// 0x30
    NO, ' ', NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, '7',	// 0x40
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', NO, NO, NO, NO,	// 0x50
    [0xC7] KEY_HOME,[0x9C] '\n' /*KP_Enter */ ,
    [0xB5] '/' /*KP_Div */ ,[0xC8] KEY_UP,
    [0xC9] KEY_PGUP,[0xCB] KEY_LF,
    [0xCD] KEY_RT,[0xCF] KEY_END,
    [0xD0] KEY_DN,[0xD1] KEY_PGDN,
    [0xD2] KEY_INS,[0xD3] KEY_DEL
};

static uint8_t shiftmap[256] = {
    NO, 033, '!', '@', '#', '$', '%', '^',	// 0x00
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',	// 0x10
    'O', 'P', '{', '}', '\n', NO, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	// 0x20
    '"', '~', NO, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', NO, '*',	// 0x30
    NO, ' ', NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, '7',	// 0x40
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', NO, NO, NO, NO,	// 0x50
    [0xC7] KEY_HOME,[0x9C] '\n' /*KP_Enter */ ,
    [0xB5] '/' /*KP_Div */ ,[0xC8] KEY_UP,
    [0xC9] KEY_PGUP,[0xCB] KEY_LF,
    [0xCD] KEY_RT,[0xCF] KEY_END,
    [0xD0] KEY_DN,[0xD1] KEY_PGDN,
    [0xD2] KEY_INS,[0xD3] KEY_DEL
};

#define C(x) (x - '@')

static uint8_t ctlmap[256] = {
    NO, NO, NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, NO,
    C('Q'), C('W'), C('E'), C('R'), C('T'), C('Y'), C('U'), C('I'),
    C('O'), C('P'), NO, NO, '\r', NO, C('A'), C('S'),
    C('D'), C('F'), C('G'), C('H'), C('J'), C('K'), C('L'), NO,
    NO, NO, NO, C('\\'), C('Z'), C('X'), C('C'), C('V'),
    C('B'), C('N'), C('M'), NO, NO, C('/'), NO, NO,
    [0xC7] KEY_HOME,
    [0xB5] C('/'),[0xC8] KEY_UP,
    [0xC9] KEY_PGUP,[0xCB] KEY_LF,
    [0xCD] KEY_RT,[0xCF] KEY_END,
    [0xD0] KEY_DN,[0xD1] KEY_PGDN,
    [0xD2] KEY_INS,[0xD3] KEY_DEL
};

static uint8_t *charcode[4] = {
    normalmap,
    shiftmap,
    ctlmap,
    ctlmap
};

/*
 * Get data from the keyboard.  If we finish a character, return it.  Else 0.
 * Return -1 if no data.
 */
static int
kbd_proc_data(void *arg)
{
    static uint32_t shift;

    if ((inb(KBSTATP) & KBS_DIB) == 0)
	return -1;

    uint8_t data = inb(KBDATAP);

    if (data == 0xE0) {
	// E0 escape character
	shift |= E0ESC;
	return 0;
    } else if (data & 0x80) {
	// Key released
	data = (shift & E0ESC ? data : data & 0x7F);
	shift &= ~(shiftcode[data] | E0ESC);
	return 0;
    } else if (shift & E0ESC) {
	// Last character was an E0 escape; or with 0x80
	data |= 0x80;
	shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];

    int c = charcode[shift & (CTL | SHIFT)][data];
    if (shift & CAPSLOCK) {
	if ('a' <= c && c <= 'z')
	    c += 'A' - 'a';
	else if ('A' <= c && c <= 'Z')
	    c += 'a' - 'A';
    }

    // Process special keys
#if CRT_SAVEROWS > 0
    // Shift-PageUp and Shift-PageDown: scroll console
    if ((shift & SHIFT) && (c == KEY_PGUP || c == KEY_PGDN)) {
	cga_scroll(c == KEY_PGUP ? -CRT_ROWS : CRT_ROWS);
	return 0;
    }
#endif

    // Ctrl-Alt-Del: reboot
    if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
	cprintf("Rebooting!\n");
	machine_reboot();
    }

    // Ctrl-Alt-Home: pause/resume profiling
    if (!(~shift & (CTL | ALT)) && c == KEY_HOME) {
	prof_toggle();
	return 0;
    }

    // Ctrl-Alt-End: halt
    if (!(~shift & (CTL | ALT)) && c == KEY_END)
	panic("halt requested");

    return c;
}

static void
kbd_intr(void *arg)
{
    cons_intr(kbd_proc_data, arg);
}

static void
kbd_init(void)
{
    // Drain the kbd buffer so that Bochs generates interrupts.
    kbd_intr(0);

    static struct interrupt_handler ih = {.ih_func = &kbd_intr };
    irq_register(1, &ih);
}


void
cgacons_init(void)
{
    static struct cons_device cgacons_cd = {
	.cd_pollin = &kbd_proc_data,
	.cd_output = &cga_putc,
    };

    cga_init();
    kbd_init();

    cons_register(&cgacons_cd);
}
