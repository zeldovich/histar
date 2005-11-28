#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H

/* console.c */
int	iscons(int fd);
int	getchar();
int	putchar(int c);

/* readline.c */
char*	readline(const char *prompt);

#endif
