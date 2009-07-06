int	smd_tty_open(int);
void	smd_tty_close(int);
int	smd_tty_read(int, unsigned char *, int);
int	smd_tty_write(int, const unsigned char *, int);
int	smd_tty_init(void);
