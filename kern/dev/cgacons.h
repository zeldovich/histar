#ifndef JOS_DEV_CGACONS_H
#define JOS_DEV_CGACONS_H

void cgacons_init(void);

#define MONO_BASE   0x3B4
#define MONO_BUF    0xB0000
#define CGA_BASE    0x3D4
#define CGA_BUF     0xB8000

#define CRT_ROWS    25
#define CRT_COLS    80
#define CRT_SIZE    (CRT_ROWS * CRT_COLS)

// Scrollback support
#define CRT_SAVEROWS    1024

/* Colors 0-7 can be used for foreground and background colors */
#define CGA_BLACK	0
#define CGA_BLUE	1
#define CGA_GREEN	2
#define CGA_CYAN	3
#define CGA_RED		4
#define CGA_MAGENTA	5
#define CGA_BROWN	6
#define CGA_WHITE	7

/* Colors 8-15 can be used for foreground only */
#define CGA_DARKGREY	8
#define CGA_LT_BLUE	9
#define CGA_LT_GREEN	10
#define CGA_LT_CYAN	11
#define CGA_LT_RED	12
#define CGA_LT_MAGENTA	13
#define CGA_YELLOW	14
#define CGA_BRITE_WHITE	15

#define CGA_BG_SHIFT	4

#endif
