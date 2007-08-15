#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>

int
main(int ac, char **av)
{
    uint32_t i = 0;

    for (;;) {
	printf("Creating socket %d..\n", i);

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	    perror("socket");
	close(fd);
	i++;
    }
}
