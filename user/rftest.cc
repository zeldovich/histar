extern "C" {
#include <inc/lib.h>    
#include <inc/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <inc/assert.h>
#include <fcntl.h>

int remfile_open(char *host, int port, char *path);
}

#include <inc/labelutil.hh>

#include <lib/dis/proxydclnt.hh>

static void
usage(char *com)
{
    printf("usage: %s port [ip]\n", com);
    exit(-1);    
}

char buf0[2000];
char buf1[2000];

static int
rand_max(int max)
{
    return 1 + (int) ((float)max * rand() / (RAND_MAX + 1.0));   
}

static void
rand_buf(char *buf, int n)
{
    for (int i = 0; i < n; i++)
        buf[i] = 1 + (int) (255.0 * rand() / (RAND_MAX + 1.0));    
}

static void
test0(int fd0, int fd1)
{
    static const int iters = 100;
    int r0 = read(fd0, buf0, sizeof(buf0));
    int r1 = read(fd1, buf1, sizeof(buf1));
    // identical starting contents
    assert(r0 == r1);
    assert(!memcmp(buf0, buf1, r0));
    int total = r0;
    
    for (int i = 0; i < iters; i++) {
        int sk = rand_max(total);
        r0 = lseek(fd0, sk, SEEK_SET);
        r1 = lseek(fd1, sk, SEEK_SET);
        assert(r0 == r1);
        r0 = rand_max(1000);
        rand_buf(buf0, r0);
        r0 = write(fd0, buf0, r0);
        r1 = write(fd1, buf0, r0);
        assert(r0 == r1);
        total += r0;
    }
    
    r0 = lseek(fd0, 0, SEEK_SET);
    r1 = lseek(fd1, 0, SEEK_SET);
    assert(r0 == r1);
    while (1) {
        r0 = read(fd0, buf0, sizeof(buf0));
        r1 = read(fd1, buf1, sizeof(buf1));
        assert(r1 == r0);
        assert(!memcmp(buf0, buf1, r0));
        if ((uint64_t)r0 < sizeof(buf0));
            break;
    }
}

int
main (int ac, char **av)
{
    if (ac < 2)
        usage(av[0]);

    int port = atoi(av[1]);
    int rfd = remfile_open((char*)"127.0.0.1", port, (char*)"test.txt"); 
    int fd = open("/x/test.txt", O_RDWR);
    assert(rfd > 0);
    assert(fd > 0);

    test0(rfd, fd);
    
    /*    
    struct stat st;
    if (fstat(fd, &st) < 0)
        printf("fstat error!\n");
    else
        printf("size %ld\n", st.st_size);
    */
    
    return 0;    
}
