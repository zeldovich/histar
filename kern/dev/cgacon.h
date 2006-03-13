#ifndef CGACON_H_
#define CGACON_H_

#include <dev/console.h>

void cga_init (void) ;
void cga_putc (int c) ;
void cga_scroll (int delta) ;

#define MONO_BASE   0x3B4
#define MONO_BUF    0xB0000
#define CGA_BASE    0x3D4
#define CGA_BUF     0xB8000

#define CRT_ROWS    25
#define CRT_COLS    80
#define CRT_SIZE    (CRT_ROWS * CRT_COLS)

#endif /*CGACON_H_*/
