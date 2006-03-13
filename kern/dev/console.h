/* See COPYRIGHT for copyright information. */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <machine/thread.h>

void cons_init(void);
void cons_putc (int c) ;
int cons_getc (void) ;


void putchar (int c);
int  getchar (void);
int  iscons (int fd);

// XXX
struct tty
{
    void (*tty_putc) (struct tty *tty, int c) ;
    void (*tty_write) (struct tty *tty, const char *b, int n) ;
    // need 'virtual console' specific data
    // need termios stuff...?  Don't think it will matter for Links...
    // vt example:
    // drivers/char/vt.c
} ;


void tty_write (const char *b, int n) ;
void tty_putc (int c) ;

extern struct Thread_list console_waiting;


#endif /* _CONSOLE_H_ */
