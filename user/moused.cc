extern "C" {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/fd.h>
#include <inc/string.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>

int mouse_fd;

void
print_pkt(uint8_t pkt[3])
{
    printf("Packet: 0x%02x%02x%02x\n", pkt[0], pkt[1], pkt[2]);
    printf("Ok?   : %d\n", pkt[0] & 0x08);
    printf("Left  : %d\n", pkt[0] & 0x04 != 0);
    printf("Middle: %d\n", pkt[0] & 0x02 != 0);
    printf("Right : %d\n", pkt[0] & 0x01 != 0);
    int x = pkt[1];
    int y = pkt[2];

    x += (pkt[0] & 0x40) ? 256 : 0;
    y += (pkt[0] & 0x80) ? 256 : 0;
    x = (pkt[0] & 0x10) ? -x : x;
    y = (pkt[0] & 0x20) ? -y : y;

    printf("x     : %d\n", x);
    printf("y     : %d\n\n", y);
}

int
main(int ac, char **av)
try
{
    /*
    if (ac != 4) {
	fprintf(stderr, "Usage: %s taint grant mousedevpath\n", av[0]);
	exit(-1);
    }
    */
    if (ac != 2) {
	fprintf(stderr, "Usage: %s mousedevpath\n", av[0]);
	exit(-1);
    }

    /*
    uint64_t taint, grant;
    error_check(strtou64(av[1], 0, 10, &taint));
    error_check(strtou64(av[2], 0, 10, &grant));
    */

    mouse_fd = open(av[1], O_RDWR);
    if (mouse_fd < 0) {
        fprintf(stderr, "Couldn't open mouse at %s\n", av[3]);
        exit(-1);
    }

    uint8_t lbuf[128];
    uint8_t lcnt = 0;

    while (true) {
        usleep(100000);
        uint8_t pkt[3];
        ssize_t i = read(mouse_fd, pkt, 3);
        if (i <= 0) {
            continue;
        }
        assert(i == 3);
        print_pkt(pkt);
        lbuf[lcnt++] = pkt[0];
        lbuf[lcnt++] = pkt[1];
        lbuf[lcnt++] = pkt[2];
        if (lcnt > 64) {
            for (int j = 0; j < lcnt; j++)
                printf("%02x", lbuf[j]);
            printf("\n");
            lcnt = 0;
        }
    }

    return 0;
} catch (std::exception &e) {
    fprintf(stderr, "%s: %s\n", av[0], e.what());
    exit(-1);
}
