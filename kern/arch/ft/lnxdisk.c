#include <machine/lnxdisk.h>
#include <dev/disk.h>
#include <inc/error.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

uint64_t disk_bytes;
int disk_fd;

void
lnxdisk_init(const char *disk_pn)
{
    disk_fd = open(disk_pn, O_RDWR);
    if (disk_fd < 0) {
	perror("opening disk file");
	exit(-1);
    }

    struct stat st;
    if (fstat(disk_fd, &st) < 0) {
	perror("stating disk file");
	exit(-1);
    }

    disk_bytes = ROUNDDOWN(st.st_size, 512);
}

int
disk_io(disk_op op, struct kiovec *iov_buf, int iov_cnt,
	uint64_t offset, disk_callback cb, void *cbarg)
{
    return -E_IO;
}

void
disk_poll(void)
{
    printf("Hmm, disk_poll()...\n");
}

int
ide_init(struct pci_func *pcif)
{
    // Just to get the PCI code to link clean.
    return 0;
}
