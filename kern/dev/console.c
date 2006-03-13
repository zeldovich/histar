/* See COPYRIGHT for copyright information. */

#include <machine/x86.h>
#include <machine/pmap.h>
#include <machine/pchw.h>
#include <inc/kbdreg.h>

#include <dev/console.h>
#include <dev/cgacon.h>

#include <kern/intr.h>
#include <kern/lib.h>

struct tty *the_tty = 0 ;

void cons_intr (int (*proc) (void));

struct Thread_list console_waiting;

// Stupid I/O delay routine necessitated by historical PC design flaws
static void
delay (void)
{
  inb (0x84);
  inb (0x84);
  inb (0x84);
  inb (0x84);
}

/***** Serial I/O code *****/

#define COM1		0x3F8

#define COM_RX		0	// In:  Receive buffer (DLAB=0)
#define COM_TX		0	// Out: Transmit buffer (DLAB=0)
#define COM_DLL		0	// Out: Divisor Latch Low (DLAB=1)
#define COM_DLM		1	// Out: Divisor Latch High (DLAB=1)
#define COM_IER		1	// Out: Interrupt Enable Register
#define   COM_IER_RDI	0x01	//   Enable receiver data interrupt
#define COM_IIR		2	// In:  Interrupt ID Register
#define COM_FCR		2	// Out: FIFO Control Register
#define COM_LCR		3	// Out: Line Control Register
#define	  COM_LCR_DLAB	0x80	//   Divisor latch access bit
#define	  COM_LCR_WLEN8	0x03	//   Wordlength: 8 bits
#define COM_MCR		4	// Out: Modem Control Register
#define	  COM_MCR_RTS	0x02	// RTS complement
#define	  COM_MCR_DTR	0x01	// DTR complement
#define	  COM_MCR_OUT2	0x08	// Out2 complement
#define COM_LSR		5	// In:  Line Status Register
#define   COM_LSR_DATA	0x01	//   Data available
#define   COM_LSR_TXRDY	0x20	//   Transmit buffer avail
#define   COM_LSR_TSRE	0x40	//   Transmitter off

static bool_t serial_exists;
bool_t output2lpt = 0;
bool_t output2com = 0;

static int
serial_proc_data (void)
{
  if (!(inb (COM1 + COM_LSR) & COM_LSR_DATA))
    return -1;
  return inb (COM1 + COM_RX);
}

static void
serial_intr (void)
{
  if (serial_exists)
    cons_intr (serial_proc_data);
}

static void
serial_putc (int c)
{
  // XXX disgusting
  if (c == '\r')
    serial_putc('\n');

  int i;

  for (i = 0; !(inb (COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800; i++)
    delay ();
  outb (COM1 + COM_TX, c);
}

static void
serial_init (void)
{
  // Turn off the FIFO
  outb (COM1 + COM_FCR, 0);

  // Set speed; requires DLAB latch
  outb (COM1 + COM_LCR, COM_LCR_DLAB);
  outb (COM1 + COM_DLL, (uint8_t) (115200 / 9600));
  outb (COM1 + COM_DLM, 0);

  // 8 data bits, 1 stop bit, parity off; turn off DLAB latch
  outb (COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

  // No modem controls
  outb (COM1 + COM_MCR, 0);
  // Enable rcv interrupts
  outb (COM1 + COM_IER, COM_IER_RDI);

  // Clear any preexisting overrun indications and interrupts
  // Serial port doesn't exist if COM_LSR returns 0xFF
  serial_exists = (inb (COM1 + COM_LSR) != 0xFF);
  (void) inb (COM1 + COM_IIR);
  (void) inb (COM1 + COM_RX);

  // Enable serial interrupts
  if (serial_exists) {
    static struct interrupt_handler ih = { .ih_func = &serial_intr };
    irq_register(4, &ih);
  }
}



/***** Parallel port output code *****/
// For information on PC parallel port programming, see the class References
// page.

static void
lpt_putc (int c)
{
  int i;

  for (i = 0; !(inb (0x378 + 1) & 0x80) && i < 12800; i++)
    delay ();
  outb (0x378 + 0, c);
  outb (0x378 + 2, 0x08 | 0x04 | 0x01);
  outb (0x378 + 2, 0x08);
}

static void
lpt_intr(void)
{
    // do nothing
}

static void
lpt_init(void)
{
    static struct interrupt_handler ih = { .ih_func = &lpt_intr };
    irq_register (7, &ih);
}

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
  [0x97] KEY_HOME,[0x9C] '\n' /*KP_Enter */ ,
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
  [0x97] KEY_HOME,[0x9C] '\n' /*KP_Enter */ ,
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
  C ('Q'), C ('W'), C ('E'), C ('R'), C ('T'), C ('Y'), C ('U'), C ('I'),
  C ('O'), C ('P'), NO, NO, '\r', NO, C ('A'), C ('S'),
  C ('D'), C ('F'), C ('G'), C ('H'), C ('J'), C ('K'), C ('L'), NO,
  NO, NO, NO, C ('\\'), C ('Z'), C ('X'), C ('C'), C ('V'),
  C ('B'), C ('N'), C ('M'), NO, NO, C ('/'), NO, NO,
  [0x97] KEY_HOME,
  [0xB5] C ('/'),[0xC8] KEY_UP,
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
kbd_proc_data (void)
{
  int c;
  uint8_t data;
  static uint32_t shift;

  if ((inb (KBSTATP) & KBS_DIB) == 0)
    return -1;

  data = inb (KBDATAP);

  if (data == 0xE0) {
    // E0 escape character
    shift |= E0ESC;
    return 0;
  }
  else if (data & 0x80) {
    // Key released
    data = (shift & E0ESC ? data : data & 0x7F);
    shift &= ~(shiftcode[data] | E0ESC);
    return 0;
  }
  else if (shift & E0ESC) {
    // Last character was an E0 escape; or with 0x80
    data |= 0x80;
    shift &= ~E0ESC;
  }

  shift |= shiftcode[data];
  shift ^= togglecode[data];

  c = charcode[shift & (CTL | SHIFT)][data];
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
    cga_scroll (c == KEY_PGUP ? -CRT_ROWS : CRT_ROWS);
    return 0;
  }
#endif

  // Ctrl-Alt-Del: reboot
  if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
    cprintf ("Rebooting!\n");
    machine_reboot();
  }

#if 0
  // Ctrl-Alt-INS: restart
  if (!(~shift & (CTL | ALT)) && c == KEY_INS) {
    // this is very dangerous... but might work.  
    extern void init (void);
    init ();
  }
#endif

#if 0
  if (!(~shift & (CTL | ALT)) && c == KEY_END) {
    monitor (NULL);
  }
#endif

  return c;
}

static void
kbd_intr (void)
{
  cons_intr (kbd_proc_data);
}

static void
kbd_init (void)
{
  // Drain the kbd buffer so that Bochs generates interrupts.
  kbd_intr ();
  static struct interrupt_handler ih = { .ih_func = &kbd_intr };
  irq_register (1, &ih);
}



/***** General device-independent console code *****/
// Here we manage the console input buffer,
// where we stash characters received from the keyboard or serial port
// whenever the corresponding interrupt occurs.

#define CONSBUFSIZE 512

static struct
{
  uint8_t buf[CONSBUFSIZE];
  uint32_t rpos;
  uint32_t wpos;
} cons;

// called by device interrupt routines to feed input characters
// into the circular console input buffer.
void
cons_intr (int (*proc) (void))
{
  int c;
  int new = 0;
  int i;

  while ((c = (*proc) ()) != -1) {
    if (c == 0)
      continue;
    cons.buf[cons.wpos++] = c;
    new++;
    if (cons.wpos == CONSBUFSIZE)
      cons.wpos = 0;
  }

  // Wake up as many processes as we might be able to serve now
  for (i = 0; i < new && !LIST_EMPTY(&console_waiting); i++) {
    struct Thread *t = LIST_FIRST(&console_waiting);
    thread_set_runnable(t);
  }
}

// return the next input character from the console, or 0 if none waiting
int
cons_getc (void)
{
  int c;

  // poll for any pending input characters,
  // so that this function works even when interrupts are disabled
  // (e.g., when called from the kernel monitor).
  serial_intr ();
  kbd_intr ();

  // grab the next character from the input buffer.
  if (cons.rpos != cons.wpos) {
    c = cons.buf[cons.rpos++];
    if (cons.rpos == CONSBUFSIZE)
      cons.rpos = 0;
    return c;
  }
  return 0;
}

// output a character to the console
void
cons_putc (int c)
{
  if (output2lpt)
    lpt_putc (c);
  if (output2com)
    serial_putc (c);
  cga_putc (c);
}

// initialize the console devices
void
cons_init (void)
{
  uint64_t output_start;

  cga_init ();
  kbd_init ();
  serial_init ();
  lpt_init ();

  LIST_INIT (&console_waiting);

  output_start = read_tsc ();
  lpt_putc ('\n');
  if (read_tsc () - output_start < 0x100000)
    output2lpt = 1;

  output_start = read_tsc ();
  serial_putc ('\n');
  if (read_tsc () - output_start < 0x100000)
    output2com = 1;

  if (strstr(&boot_cmdline[0], "serial=off"))
    output2com = 0;
  if (strstr(&boot_cmdline[0], "lpt=off"))
    output2lpt = 0;

  if (!serial_exists)
    cprintf ("Serial port does not exist!\n");
}

void
tty_write (const char *b, int n)
{
    if (the_tty)
        the_tty->tty_write(the_tty, b, n) ;
    else
        cprintf("%.*s", n, b);
}

void
tty_putc (int n)
{
    if (the_tty)
        the_tty->tty_putc(the_tty, n) ;
    else
        cons_putc(n) ;
}

// `High'-level console I/O.  Used by readline and cprintf.

void
putchar (int c)
{
  cons_putc(c) ;
}

int
getchar (void)
{
  int c;

  while ((c = cons_getc ()) == 0)
    /* do nothing */ ;
  return c;
}

int
iscons (int fdnum __attribute__((unused)))
{
  // used by readline
  return 1;
}
