#include <kern/disk.h>
#include <dev/filedisk.h>
#include <inc/error.h>
#include <inc/intmacro.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

struct req {
    disk_op op;
    struct kiovec *iov_buf;
    int iov_cnt;
    uint64_t off;
    disk_callback cb;
    void *cbarg;
};

static int
filedisk_issue(struct disk *dk, disk_op op,
	       struct kiovec *iov_buf, int iov_cnt,
	       uint64_t off, disk_callback cb, void *cbarg)
{
    struct req *r = (struct req *) dk->dk_arg;
    if (!SAFE_EQUAL(r->op, op_none))
	return -E_BUSY;

    r->op = op;
    r->iov_buf = iov_buf;
    r->iov_cnt = iov_cnt;
    r->off = off;
    r->cb = cb;
    r->cbarg = cbarg;
    return 0;
}

static void
filedisk_poll(struct disk *dk)
{
    struct req *r = (struct req *) dk->dk_arg;
    if (SAFE_EQUAL(r->op, op_none))
	return;

    struct iovec iov[r->iov_cnt];
    for (int i = 0; i < r->iov_cnt; i++) {
	iov[i].iov_base = r->iov_buf[i].iov_base;
	iov[i].iov_len  = r->iov_buf[i].iov_len;
    }

    int fd = dk->dk_id;
    lseek(fd, r->off, SEEK_SET);
    if (SAFE_EQUAL(r->op, op_write))
	writev(fd, &iov[0], r->iov_cnt);
    else
	readv(fd, &iov[0], r->iov_cnt);

    disk_callback cb = r->cb;
    void *cbarg = r->cbarg;
    r->op = op_none;

    cb(disk_io_success, cbarg);
}

void
filedisk_init(const char *disk_pn)
{
    int fd = open(disk_pn, O_RDWR);
    if (fd < 0) {
	perror("opening disk file");
	exit(-1);
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
	perror("stating disk file");
	exit(-1);
    }

    struct disk *dk = malloc(sizeof(*dk));
    snprintf(dk->dk_model, sizeof(dk->dk_model), "%s", disk_pn);
    snprintf(dk->dk_serial, sizeof(dk->dk_serial), "none");
    snprintf(dk->dk_firmware, sizeof(dk->dk_firmware), "filedisk");
    snprintf(dk->dk_busloc, sizeof(dk->dk_busloc), "filedisk");
    dk->dk_bytes = ROUNDDOWN(st.st_size, 512);
    dk->dk_poll  = filedisk_poll;
    dk->dk_issue = filedisk_issue;
    dk->dk_id    = fd;

    struct req *r = malloc(sizeof(*r));
    r->op = op_none;
    dk->dk_arg = r;

    disk_register(dk);
}
