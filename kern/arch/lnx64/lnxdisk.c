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
disk_io(disk_op op __attribute__((unused)),
	struct kiovec *iov_buf __attribute__((unused)),
	int iov_cnt __attribute__((unused)),
        uint64_t offset __attribute__((unused)),
	disk_callback cb __attribute__((unused)),
	void *cbarg __attribute__((unused)))
{
    return -E_IO;
}

void
ide_intr(void)
{
    printf("Hmm, ide_intr()...\n");
}

void
disk_init(struct pci_func *pcif)
{
    // Just to get the PCI code to link clean.
}
