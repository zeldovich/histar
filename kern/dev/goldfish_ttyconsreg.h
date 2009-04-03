#ifndef JOS_DEV_GOLDFISH_TTYCONSREG
#define JOS_DEV_GOLDFISH_TTYCONSREG

struct goldfish_ttycons_reg {
	volatile uint32_t put_char;
	volatile uint32_t bytes_ready;
	volatile uint32_t command;
	volatile uint32_t data_ptr;
	volatile uint32_t data_len;
}; 

#define GF_TTY_CMD_INT_DISABLE	0
#define GF_TTY_CMD_INT_ENABLE	1
#define GF_TTY_CMD_WRITE_BUF	2
#define GF_TTY_CMD_READ_BUF	3

#endif /* !JOS_DEV_GOLDFISH_TTYCONSREG */
