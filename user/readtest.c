#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <sys/stat.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s filename\n", av[0]);
	exit(-1);
    }
    char *pn = av[1];

    struct stat st;
    assert(0 == stat(pn, &st));

    int fd = open(pn, O_RDONLY);
    if (fd < 0)
	perror("open");

    char *buf = malloc(st.st_size);
    assert(st.st_size == read(fd, buf, st.st_size));

    uint64_t xbuf_size = 8192;
    char xbuf[xbuf_size];

    for (int i = 0; i < 1000; i++) {
	int64_t offset = rand() % (st.st_size - xbuf_size);
	int64_t count = rand() % xbuf_size;

	assert(offset == lseek(fd, offset, SEEK_SET));
	assert(count == read(fd, xbuf, count));

	if (memcmp(&xbuf[0], &buf[offset], count))
	    printf("read data mismatch\n");
    }

    printf("done.\n");
}
