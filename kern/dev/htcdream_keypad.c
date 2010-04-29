#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/timer.h>
#include <kern/console.h>
#include <dev/msm_gpio.h>
#include <dev/htcdream_backlight.h>
#include <dev/htcdream_keypad.h>
#include <inc/kbdcodes.h>

#define REPORT_UNMAPPED_KEYS	/* output row and column of unmapped keys */

static int gpio_cols[] = { 35, 34, 33, 32, 31, 23, 30, 109 };
static int gpio_rows[] = { 42, 41, 40, 39, 38, 37, 36 };

#define NGPIO_COLS ((int)(sizeof(gpio_cols) / sizeof(gpio_cols[0])))
#define NGPIO_ROWS ((int)(sizeof(gpio_rows) / sizeof(gpio_rows[0])))

#define SHIFT	0x1	// bogus
#define ALT	0x2	// bogus
#define BACKSPC	0x8	// ascii backspace

// NB: - Internal MENU (row 5, col 1) is mapped as KEY_F1.
//     - Internal magnifying glass (row 2, col 7) is mapped as KEY_F2. 
//     - The following external buttons are unmapped:
//       - BACK (row 0, col 0)
//	 - MENU (row 0, col 1)
//	 - HOME (row 1, col 0)
//	 - SEND (row 1, col 1) [the green phone key] 
//	 - VOLUME UP (row 3, col 5)	-- hacked: backlight on
//	 - VOLUME DOWN (row 3, col 7)	-- hacked: backlight off

static const uint8_t keymap_normal[NGPIO_ROWS][NGPIO_COLS] = {
	{  0,         0,       'u',  '5',  '4',  '2',  'i',  '3'      }, 
	{  0,         0,       '7',  '6',  'r',  'w',  '0',  'e'      },
	{  0,         0,       'k',  'b',  'v',   ALT, 'o',   KEY_F2  },
	{  BACKSPC,   SHIFT,   'j',  'h',  'g',   0,   'l',   0       },
	{ '\n',      'a',      'm',  'n',  'c',  's',  ',',  'x'      },
	{  SHIFT,     KEY_F1,  '.',  ' ',  '@',  'z',   ALT, 'f'      },
	{ 'p',       'q',      '8',  'y',  't',  '1',  '9',  'd'      }
};

static const uint8_t keymap_shifted[NGPIO_ROWS][NGPIO_COLS] = {
	{  0,         0,       'U',  '%',  '$',  '@',  'I',  '#'      }, 
	{  0,         0,       '&',  '^',  'R',  'W',  ')',  'E'      },
	{  0,         0,       'K',  'B',  'V',   ALT, 'O',   KEY_F2  },
	{  BACKSPC,   SHIFT,   'J',  'H',  'G',   0,   'L',   0       },
	{ '\n',      'A',      'M',  'N',  'C',  'S',  '?',  'X'      },
	{  SHIFT,     KEY_F1,  '/',  ' ',  '~',  'Z',   ALT, 'F'      },
	{ 'P',       'Q',      '*',  'Y',  'T',  '!',  '(',  'D'      }
};

static const uint8_t keymap_alted[NGPIO_ROWS][NGPIO_COLS] = {
	{  0,         0,       'x',  '%',  '$',  '@',  '-',  '#'      }, 
	{  0,         0,       '&',  '^',  'R',  '\'', ')',  '_'      },
	{  0,         0,       '"',  ']',  '[',  ALT,  '+',   KEY_F2  },
	{  BACKSPC,   SHIFT,   ';',  ':',  '}',   0,   'L',   0       },
	{ '\n',      'A',      '>',  '<',  'C',  '|',  '?',  'X'      },
	{  SHIFT,     KEY_F1,  '/',  ' ',  '~',  'Z',  ALT,  '{'      },
	{ '=',       '\t',     '*',  '/',  'T',  '!',  '(',  '\\'     }
};

static uint8_t last_key_down;	// 1 => most recently hit key is pressed

static uint8_t get_key(void);

#if 0	/* useful for debugging/seeing what keys map to */
static void
print_scan_matrix()
{
	while (1) {
		cprintf("--------------------------------------------------\n");
		cprintf("     %3d   %3d   %3d   %3d   %3d   %3d   %3d   %3d\n",
		    gpio_cols[0], gpio_cols[1], gpio_cols[2], gpio_cols[3],
		    gpio_cols[4], gpio_cols[5], gpio_cols[6], gpio_cols[7]);

		for (int i = 0; i < NGPIO_ROWS; i++) {
			const char *cX[NGPIO_COLS];

			msm_gpio_set_direction(gpio_rows[i],GPIO_DIRECTION_OUT);
			for (int j = 0; j < NGPIO_COLS; j++) {
				if (!msm_gpio_read(gpio_cols[j]))
					cX[j] = " X";
				else
					cX[j] = "  ";
			}
			msm_gpio_set_direction(gpio_rows[i], GPIO_DIRECTION_IN);

			cprintf("%3d   %s    %s    %s    %s    %s    %s    %s"
			    "    %s\n", gpio_rows[i], cX[0], cX[1], cX[2],
			    cX[3], cX[4], cX[5], cX[6], cX[7]);
		}

		/* delay 1 sec */
		timer_delay(1000 * 1000 * 1000);	
	}
}
#endif

static uint8_t
get_key()
{
	int row = -1, col = -1;
	int shifted = 0, alted = 0;
	uint8_t key;

	/* NB: we only detect one non-shift simultaneous key press */
	for (int i = 0; i < NGPIO_ROWS; i++) {
		msm_gpio_set_direction(gpio_rows[i], GPIO_DIRECTION_OUT);
		for (int j = 0; j < NGPIO_COLS; j++) {
			if (!msm_gpio_read(gpio_cols[j])) {
				if (keymap_normal[i][j] == SHIFT)
					shifted = 1;
				else if (keymap_normal[i][j] == ALT)
					alted = 1;
				else
					row = i, col = j;
			}
		}
		msm_gpio_set_direction(gpio_rows[i], GPIO_DIRECTION_IN);
		timer_delay(100 * 1000); // wait for capacitance to drain
	}

	if (row == -1 && col == -1)
		return (0);

	key = keymap_normal[row][col];
	if (shifted)
		key = keymap_shifted[row][col];
	if (alted)
		key = keymap_alted[row][col];

#ifdef REPORT_UNMAPPED_KEYS
	if (key == 0) {
		cprintf("%s: unimplemented key at row %d, col %d\n", __FILE__,
		    row, col); 
	}
#endif
	// XXX
	if (key == 0 && row == 3 && col == 5)
		htcdream_backlight_level(100);	
	if (key == 0 && row == 3 && col == 7)
		htcdream_backlight_level(0);


	return (key);
}

static void
htcdream_keypad_putc(void *arg, int c, cons_source src)
{
	/* uh, right... */
}

/*
 * Get data from the keyboard.  If we finish a character, return it.  Else 0.
 * Return -1 if no data.
 */
static int
htcdream_keypad_proc_data(void *arg)
{
	uint8_t key = get_key();

	if (key == 0) {
		last_key_down = 0;
		return (-1);
	}

	if (key == last_key_down)
		return (-1);

	last_key_down = key;

	return (key);
}

static void
keypad_timer()
{
	cons_intr(htcdream_keypad_proc_data, NULL);
}

void
htcdream_keypad_init(uint32_t board_rev)
{
	static struct periodic_task htcdream_keypad_timer;
	static struct cons_device htcdream_keypad_cd = {
		.cd_pollin = &htcdream_keypad_proc_data,
		.cd_output = &htcdream_keypad_putc,
	};

	// set our GPIO pins for input: output->low, output enable->off
	for (int i = 0; i < NGPIO_COLS; i++) {
		msm_gpio_write(gpio_cols[i], 0);
		msm_gpio_set_direction(gpio_cols[i], GPIO_DIRECTION_IN);
	}
	for (int i = 0; i < NGPIO_ROWS; i++) {
		msm_gpio_write(gpio_rows[i], 0);
		msm_gpio_set_direction(gpio_rows[i], GPIO_DIRECTION_IN);
	}

	htcdream_keypad_timer.pt_interval_msec = 10;
	htcdream_keypad_timer.pt_fn = keypad_timer;
	timer_add_periodic(&htcdream_keypad_timer);

	cons_register(&htcdream_keypad_cd);
}
