// LFS small file benchmark

#ifdef JOS_USER
#define JOS64 1
#define LINUX 0
#else
#define JOS64 0
#define LINUX 1
#endif

#if JOS64
#include <inc/string.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/syscall.h>

#define O_TRUNC         0x00000008

#define S_IRWXU         0x00000001

#define umask(a)
#define fsync(fd) 
#define time_msec() sys_clock_msec()
#endif // JOS64

#if LINUX
#include "sys/types.h"
#include "sys/stat.h"
#include <sys/timeb.h>
#include <errno.h>
#include "fcntl.h"

#define time_msec() ({ struct timeval tv; gettimeofday(&tv, 0); tv.tv_sec * 1000 + tv.tv_usec / 1000; })
#endif // LINUX

static char buf[40960];
static char name[32];
static char *prog_name;

#define NDIR 100


static char dir[32];

void 
creat_dir()
{
    int i;

    umask(0);

    for (i = 0; i < NDIR; i++) {
        sprintf(dir, "d%d", i);
        mkdir(dir, 0777);
    }
}



void
creat_test(int n, int size)
{
    int i;
    int r;
    int fd;
    int j;

    unsigned s = 0 , f = 0 ;
    s = time_msec();

    for (i = 0, j = 0; i < n; i ++) {

		sprintf(name, "d%d/g%d", j, i);
	
		if((fd = open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
		    printf("creat_test: create %d failed: %d\n", i, fd);
		    exit(1);
		}
	
		if ((r = write(fd, buf, size)) < 0) {
		    printf("creat_test: write failed: %d\n", r);
		    exit(1);
		}
	
		if ((r = close(fd)) < 0) {
		    printf("creat_test: close failed %d\n", r);
		}
	
		if ((i+1) % 100 == 0) j++;

    }

    fsync(fd);

    f = time_msec();
    printf("%s: creat took %d msec\n",  prog_name,  f - s);
}


void
write_test(char *name, int n, int size)
{
    int i = 0 ;
    int r;
    int fd;
    unsigned s = 0 , f = 0 ;
    
    s = time_msec() ;
    
    if((fd = open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
		printf("write_test: create of %s failed: %dn", name, fd);
		exit(1);
    }

    for (i = 0; i < n; i ++) {
		if ((r = write(fd, buf, size)) < 0) {
		    printf("write_test: write to %s failed: %d\n", name, r);
		    exit(1);
		}
    }
    
    if ((r = close(fd)) < 0) {
		printf("write_test: close failed: %d\n", r);
    }
    
    f = time_msec() ;
    
    printf("write_test: write took %d msec\n", f - s);
}


void
flush_cache()
{
    write_test("t", 20000, 4096);
}

void
read_test(int n, int size)
{
    int i;
    int r;
    int fd;
    int j;

    unsigned s = 0 , f = 0 ;
    s = time_msec();
    for (i = 0, j = 0; i < n; i ++) {

		sprintf(name, "d%d/g%d", j, i);
	
		if((fd = open(name, O_RDONLY, 0)) < 0) {
		    printf("read_test: open %d failed %d\n", i, fd);
		    exit(1);
		}
	
		if ((r = read(fd, buf, size)) < 0) {
		    printf("read_test: read failed %d\n", r);
		    exit(1);
		}
	
		if ((r = close(fd)) < 0) {
		    printf("read_test: close failed %d\n", r);
		}

		if ((i+1) % 100 == 0) j++;
    }

	f = time_msec();
    printf("read_test: read took %d msec\n", f - s);
}

void 
delete_test(int n)
{	
    int i;
    int r;
    int j;
 
    unsigned s = 0 , f = 0;
    s = time_msec();
    for (i = 0, j = 0; i < n; i ++) {

	sprintf(name, "d%d/g%d", j, i);

	if ((r = unlink(name)) < 0) {
	    printf("%s: unlink failed %d\n", prog_name, r);
	    exit(1);
	}

	if ((i+1) % 100 == 0) j++;
    }

    //fsync(fd);

    f = time_msec();
    printf("delete_test: unlink took %d msec\n", f - s);
}


int 
main(int argc, char *argv[])
{
    int n;
    int size;

    prog_name = argv[0];

    if (argc != 3) {
	printf("%s: %s num size\n", prog_name, prog_name);
	exit(1);
    }

    n = atoi(argv[1]);
    size = atoi(argv[2]);

    printf("%s %d %d\n", prog_name, n, size);

    creat_dir();

    creat_test(n, size);
    read_test(n, size);
    delete_test(n);

    unlink("t");
}
