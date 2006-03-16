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
int cons_probe (void) ;
void cons_cursor (int row, int col) ;

void putchar (int c);
int  getchar (void);
int  iscons (int fd);

extern struct Thread_list console_waiting;

#define MONO_BASE   0x3B4
#define MONO_BUF    0xB0000
#define CGA_BASE    0x3D4
#define CGA_BUF     0xB8000

#define CRT_ROWS    25
#define CRT_COLS    80
#define CRT_SIZE    (CRT_ROWS * CRT_COLS)

// Scrollback support
#define CRT_SAVEROWS    1024


#endif /* _CONSOLE_H_ */
