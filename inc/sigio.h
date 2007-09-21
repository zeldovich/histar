#ifndef JOS_INC_SIGIO_H
#define JOS_INC_SIGIO_H

void jos_sigio_enable(int fd);
void jos_sigio_activate(int fd);
void jos_sigio_disable(int fd);

#endif
