extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   
#include <assert.h>
#include <fcntl.h>
}

#include <lib/dis/fileclient.hh>

static int
read(fileclient *fc, char *buf, int n)
{
    const file_frame *frame = fc->frame_at(n, fc->frame().offset_);
    int cc = MIN(n, frame->count_);
    memcpy(buf, frame->byte_, cc);
    return cc;        
}

static int
write(fileclient *fc, char *buf, int n)
{
    const file_frame *frame = fc->frame();
    frame = fc->frame_at_is(buf, n, frame->offset_);
    return frame->count_;        
}

static int
lseek(fileclient *fc, int offset, int whence)
{
    file_frame *frame = (file_frame *)fc->frame();
    frame->offset_ = offset;
    return offset;        
}

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

char buf0[2000];
char buf1[2000];

static void
test0(fileclient *fc0, int fd1)
{
    static const int iters = 10;
    int r0 = read(fc0, buf0, sizeof(buf0));
    int r1 = read(fd1, buf1, sizeof(buf1));
    // identical starting contents
    assert(r0 == r1);
    assert(!memcmp(buf0, buf1, r0));
    int total = r0;
    
    for (int i = 0; i < iters; i++) {
        int sk = rand_max(total);
        r0 = lseek(fc0, sk, SEEK_SET);
        r1 = lseek(fd1, sk, SEEK_SET);
        assert(r0 == r1);
        r0 = rand_max(1000);
        rand_buf(buf0, r0);
        r0 = write(fc0, buf0, r0);
        r1 = write(fd1, buf0, r0);
        assert(r0 == r1);
        total += r0;
    }
    
    r0 = lseek(fc0, 0, SEEK_SET);
    r1 = lseek(fd1, 0, SEEK_SET);
    assert(r0 == r1);
    while (1) {
        r0 = read(fc0, buf0, sizeof(buf0));
        r1 = read(fd1, buf1, sizeof(buf1));
        assert(r1 == r0);
        assert(!memcmp(buf0, buf1, r0));
        if ((uint64_t)r0 < sizeof(buf0));
            break;
    }
}

static void 
usage(char *com)
{
    printf("usage: %s port\n", com);    
    exit(-1);
}

int
main (int ac, char **av)
{
    if (ac < 2)
        usage(av[0]);
    int port = atoi(av[1]);

    fileclient *fc = new fileclient("/tmp/test0", "127.0.0.1", port);
    const file_frame *frame = fc->frame_at(10, 0);
    printf("0) frame->count %ld frame->offset %ld\n", frame->count_, frame->offset_);
    delete fc;

    fc = new fileclient("/tmp/test1", "127.0.0.1", port);
    frame = fc->frame_at(10, 0);
    printf("1) frame->count %ld frame->offset %ld\n", frame->count_, frame->offset_);
    delete fc;
    
    fc = new fileclient("/tmp/test0", "127.0.0.1", port);
    int fd = open("test0", O_RDWR);
    test0(fc, fd);

    return 0;    
}
