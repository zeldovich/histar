#ifndef LINUX_ARCH_LIND_JIF_H
#define LINUX_ARCH_LIND_JIF_H

struct jif_list {
    char name[16];
    unsigned char mac[6];
    unsigned int data_len;
};

/* jif_user.c */
int jif_list(struct jif_list *list, unsigned int cnt);
int jif_open(const char *name, void *data);
int jif_tx(void *data, void *buf, unsigned int buf_len);
int jif_rx(void *data, void *buf, unsigned int buf_len);
void jif_irq_reset(void *data);

#endif
