#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "bootimg.h"

int
main(int argc, char **argv)
{
	int i, fd;
	boot_img_hdr hdr;

	if (argc != 2) {
		fprintf(stderr, "usage: %s imgfile\n", argv[0]);
		exit(1);
	}

	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		perror("read");
		exit(1);
	}

	close(fd);

	printf("BOOT IMAGE HEADER:\n");
	printf("  Magic:        [%.8s]\n", hdr.magic);
	printf("  Kern Size:    0x%08x (%u)\n", hdr.kernel_size, hdr.kernel_size);
	printf("  Kern Addr:    0x%08x\n", hdr.kernel_addr);
	printf("  Ramdisk Size: 0x%08x (%u)\n", hdr.ramdisk_size, hdr.ramdisk_size); 
	printf("  Ramdisk Addr: 0x%08x\n", hdr.ramdisk_addr);
	printf("  Second Size:  0x%08x (%u)\n", hdr.second_size, hdr.second_size); 
	printf("  Second Addr:  0x%08x\n", hdr.second_addr);
	printf("  Tags Addr:    0x%08x\n", hdr.tags_addr);
	printf("  Page Size:    %u\n", hdr.page_size); 
	printf("  Unused 0:     %u\n", hdr.unused[0]);
	printf("  Unused 1:     %u\n", hdr.unused[1]);
	printf("  Name:         [%.16s]\n", hdr.name);
	printf("  Command Line: [%.512s]\n", hdr.cmdline); 
	printf("  ID:           %d: 0x%08x\n", 0, hdr.id[0]);
	for (i = 1; i < 8; i++)
		printf("                %d: 0x%08x\n", i, hdr.id[i]);

	return (0);
}
