#ifndef JOS_INC_JCOMM_H
#define JOS_INC_JCOMM_H

#include <inc/container.h>
#include <inc/multisync.h>
#include <inc/label.h>

enum { jlink_bufsz = 9000 };
enum { jcomm_chan0 = 0, jcomm_chan1 = 1 };

struct jlink {
    char buf[jlink_bufsz];
    char reader_waiting;
    char writer_waiting;
    uint64_t open;
    uint32_t read_ptr;  /* read at this offset */
    uint64_t bytes; /* # bytes in circular buffer */
    uint16_t mode; /* default mode */
    jthread_mutex_t mu;
};

struct jcomm {
    uint64_t segment;
    char chan;
};

struct jcomm_ref {
    uint64_t container;
    struct jcomm jc;
};

#define JCOMM(container, comm) \
        ((struct jcomm_ref) { (container), (comm) })

#define JCOMM_NONBLOCK_RD 0x0001
#define JCOMM_NONBLOCK_WR 0x0002
#define JCOMM_PACKET      0x0004

#define JCOMM_SHUT_RD  0x0001
#define JCOMM_SHUT_WR  0x0002

int jcomm_alloc(uint64_t ct, struct ulabel *l, int16_t mode,
		struct jcomm_ref *a, struct jcomm_ref *b);
int jcomm_probe(struct jcomm_ref jr, dev_probe_t probe);
int jcomm_shut(struct jcomm_ref jr, uint16_t how);
int jcomm_multisync(struct jcomm_ref jr, dev_probe_t probe, struct wait_stat *wstat, int wslots_avail);

int jcomm_addref(struct jcomm_ref jr, uint64_t ct);
int jcomm_unref(struct jcomm_ref jr);

int64_t jcomm_read(struct jcomm_ref jr, void *buf, uint64_t cnt, int dowait);
int64_t jcomm_write(struct jcomm_ref jr, const void *buf, uint64_t cnt, int dowait);
int     jcomm_write_flush(struct jcomm_ref jr);

int64_t jlink_read(struct jlink *jl, void *buf, uint64_t cnt, int16_t mode);
int64_t jlink_write(struct jlink *jl, const void *buf, uint64_t cnt, int16_t mode);
int     jlink_write_flush(struct jlink *jl);

#endif
