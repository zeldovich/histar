#ifndef UCLIBC_JOS64_SIGNALGATE_H
#define UCLIBC_JOS64_SIGNALGATE_H

void signal_gate_incoming(siginfo_t *si);
void signal_gate_close(void);
void signal_gate_init(void);
int  signal_gate_send(struct cobj_ref gate, siginfo_t *si);

#endif
