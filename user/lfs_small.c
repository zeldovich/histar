// LFS small file benchmark

enum { lfs_profiling = 0 };
enum { lfs_iterations = 1 };

#ifdef JOS_USER
#define JOS64 1
#define LINUX 0
#else
#define JOS64 0
#define LINUX 1
#endif

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>

#if JOS64
#include <inc/syscall.h>
#include <inc/profiler.h>

#define time_msec() sys_clock_msec()
#endif // JOS64

#if LINUX
#include <sys/timeb.h>

#define time_msec() ({ struct timeval tv; gettimeofday(&tv, 0); tv.tv_sec * 1000 + tv.tv_usec / 1000; })
#endif // LINUX

static char buf[40960];
static char namebuf[32];
static char *prog_name;
static int syncopt;

#define NDIR 100


static char dir[32];

static void 
creat_dir()
{
    int i;

    umask(0);

    for (i = 0; i < NDIR; i++) {
        sprintf(dir, "d%d", i);
        mkdir(dir, 0777);
    }
}



static void
creat_test(int n, int size)
{
    int i;
    int r;
    int fd = 0;
    int j;

    unsigned s = 0 , f = 0 ;
    s = time_msec();

    for (i = 0, j = 0; i < n; i ++) {

		sprintf(namebuf, "d%d/g%d", j, i);
	
		if((fd = open(namebuf, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
		    printf("creat_test: create %d failed: %d\n", i, fd);
		    exit(1);
		}
	
		if ((r = write(fd, buf, size)) < 0) {
		    printf("creat_test: write failed: %d\n", r);
		    exit(1);
		}

		if (syncopt == 1 && fsync(fd) < 0) {
		    printf("creat_test: fsync failed: %s\n", strerror(errno));
		    exit(1);
		}
	
		if ((r = close(fd)) < 0) {
		    printf("creat_test: close failed %d\n", r);
		}

#if LINUX
	    if (syncopt == 1) {
		sprintf(namebuf, "d%d", j);
		if ((fd = open(namebuf, O_RDONLY)) < 0) {
		    printf("creat_test: open dir failed: %s\n", strerror(errno));
		    exit(1);
		}

		if (fsync(fd) < 0) {
		    printf("creat_test: fsync dir failed: %s\n", strerror(errno));
		    exit(1);
		}

		close(fd);
	    }
#endif

		if ((i+1) % 100 == 0) j++;
		if (j % 100 == 0) j = 0;

    }

    if (syncopt == 2)
	sync();

    f = time_msec();
    printf("%s: creat took %d msec\n",  prog_name,  f - s);
}


#if 0
static void
write_test(const char *name, int n, int size)
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
#endif


#if 0
static void
flush_cache()
{
    write_test("t", 20000, 4096);
}
#endif

static void
read_test(int n, int size)
{
    int i;
    int r;
    int fd;
    int j;

    unsigned s = 0 , f = 0 ;
    s = time_msec();
    for (i = 0, j = 0; i < n; i ++) {

		sprintf(namebuf, "d%d/g%d", j, i);
	
		if((fd = open(namebuf, O_RDONLY, 0)) < 0) {
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
		if (j % 100 == 0) j = 0;
    }

	f = time_msec();
    printf("read_test: read took %d msec\n", f - s);
}

static void 
delete_test(int n)
{	
    int i;
    int r;
    int j;
 
    unsigned s = 0 , f = 0;
    s = time_msec();
    for (i = 0, j = 0; i < n; i ++) {

	sprintf(namebuf, "d%d/g%d", j, i);

	if ((r = unlink(namebuf)) < 0) {
	    printf("%s: unlink failed %d\n", prog_name, r);
	    exit(1);
	}

#if LINUX
	if (syncopt == 1) {
		int fd;
		sprintf(namebuf, "d%d", j);
		if ((fd = open(namebuf, O_RDONLY)) < 0) {
		    printf("creat_test: open dir failed: %s\n", strerror(errno));
		    exit(1);
		}

		if (fsync(fd) < 0) {
		    printf("creat_test: fsync dir failed: %s\n", strerror(errno));
		    exit(1);
		}

		close(fd);
	}
#endif

	if ((i+1) % 100 == 0) j++;
	if (j % 100 == 0) j = 0;
    }

    //fsync(fd);

    if (syncopt == 2)
	sync();

    f = time_msec();
    printf("delete_test: unlink took %d msec\n", f - s);
}


int 
main(int argc, char *argv[])
{
    int i;
    int n;
    int size;

    prog_name = argv[0];

    if (argc != 4) {
	printf("%s: %s num size sync-opt\n", prog_name, prog_name);
	printf(" sync-opt: 0 for no-sync, 1 for file-sync, 2 for group-sync\n");
	exit(1);
    }

#ifdef JOS_USER
    if (lfs_profiling)
	profiler_init();
#endif

    n = atoi(argv[1]);
    size = atoi(argv[2]);
    syncopt = atoi(argv[3]);

    printf("%s %d %d\n", prog_name, n, size);

    creat_dir();

    for (i = 0; i < lfs_iterations; i++) {
	creat_test(n, size);
	read_test(n, size);
	delete_test(n);
    }

    unlink("t");
}
