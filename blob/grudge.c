#include <stdio.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>

uint32_t
ru32(int fd, int off)
{
	if (lseek(fd, off, SEEK_SET) == -1) {
		perror("lseek");
		exit(1);
	}
	uint32_t res;
	if (read(fd, &res, 4) != 4) {
		perror("read");
		exit(1);
	}
	return res;
}

void
wu32(int fd, int off, uint32_t val)
{
	if (lseek(fd, off, SEEK_SET) == -1) {
		perror("lseek");
		exit(1);
	}
	if (write(fd, &val, 4) != 4) {
		perror("write");
		exit(1);
	}
}

int
main()
{
	int fd = open("libgps.so", O_RDWR);
	int i;

	for (i = 0x13000; i < 0x13884; i += 4) {
		uint32_t v = ru32(fd, i);
		if ((v & 0xa9700000) == 0xa9700000) {
			uint32_t n = (v - 0xa9700000) + 0x68000000;
			printf("%x: 0x%08x --> 0x%08x\n", i, v, n);
			wu32(fd, i, n); 
		}
	}

	close(fd);
}
