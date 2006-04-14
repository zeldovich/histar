// LFS large file benchmark

#ifdef JOS_USER
#define JOS64 1
#define LINUX 0
#else
#define JOS64 0
#define LINUX 1
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>

#if JOS64
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/syscall.h>

#define time_msec() sys_clock_msec()
#endif // JOS64

#if LINUX
#include <sys/timeb.h>

#define time_msec() ({ struct timeval tv; gettimeofday(&tv, 0); tv.tv_sec * 1000 + tv.tv_usec / 1000; })
#endif // LINUX

#define seek(fd, offset) lseek(fd, offset, SEEK_SET)
#define SIZE	8192

static char buf[SIZE];
static char name[32] = "test_file";
static char *prog_name;

extern int errno;

static int
f(int i, int n)
{
    return ((i * 11) % n);
}

static void
write_test(int n, int size, int sequential)
{
    int i = 0 ;
    int r;
    int fd;
    long pos = 0 ;
    struct stat statb;

    unsigned s, fin;
    s = time_msec();
    
    if((fd = open(name, O_RDWR, 0)) < 0) {
		printf("write_test: open %s failed: %d\n", name, fd);
		exit(1);
    }

    for (i = 0; i < n; i ++) {
		if (!sequential) {
	
#ifdef TRULY_RANDOM
		    pos = (random() % n) * size;
#else
		    pos = f(i, n) * size;
#endif
	
		    if ((r = seek(fd, pos)) < 0) {
				printf("write_test: seek failed %s: %d\n", name, r);
		    }
		}
	
		if ((r = write(fd, buf, size)) < 0) {
		    printf("write_test: write failed %s: %d\n", name, r) ;
		    exit(1);
		}
    }
    
    fsync(fd);
    fstat(fd, &statb);
    if (fchown(fd, statb.st_uid, -1) < 0) {
		printf("write_test: fchown error\n");
    }

    if ((r = close(fd)) < 0) {
		printf("write_test: close failed %s: %d\n", name, r);
    }

    fin = time_msec();
    printf("write_test: write took %d msec\n", fin - s);

}


static int
g(int i, int n)
{
    if (i % 2 == 0) return(n / 2 + i / 2);
    else return(i / 2);
}


static void
read_test(int n, int size, int sequential)
{
    int i = 0 ;
    int r;
    int fd;
    long pos;


    unsigned s, fin;
    s = time_msec();
    
    if((fd = open(name, O_RDONLY, 0)) < 0) {
		printf("read_test: open %s failed: %d\n", name,fd);
		exit(1);
    }

    for (i = 0; i < n; i ++) {
		if (!sequential) {
	
#ifdef TRULY_RANDOM
		    pos = (random() % n) * size;
#else
		    pos = g(i, n) * size;
#endif
	
		    if ((r = seek(fd, pos)) < 0) {
				printf("read_test: seek failed %s: %d\n", name, r);
		    }
		}
	
		if ((r = read(fd, buf, size)) < 0) {
		    printf("read_test: read failed %s: %d\n", name, r);
		    exit(1);
		}
    }
    
    if ((r = close(fd)) < 0) {
		printf("read_test: close failed %s: %d\n", name, r);
    }
    

    fin = time_msec();
    printf("%s: read took %d msec\n",
	   prog_name,
	   fin - s);
}


static void __attribute__((unused))
flush_cache()
{
    int i = 0 ;
    int r = 0 ;
    int fd;

    if((fd = open("t", O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
		printf("flush_cache: create t failed: %d\n", fd);
		exit(1);
    }

    for (i = 0; i < 15000; i ++) {
		if ((r = write(fd, buf, 4096)) < 0) {
		    printf("flush_cache: write t failed: %d\n", r);
		    exit(1);
		}
    }
    
    fsync(fd);

    if ((r = close(fd)) < 0) {
		printf("flush_cache: close t failed: %d\n", r);
    }

    unlink("t");
}



int main(int argc, char *argv[])
{
    int n;
    int size;

    prog_name = argv[0];

    if (argc != 3) {
		printf("%s: %s num_blocks size_block\n", prog_name, prog_name);
		exit(1);
    }

    n = atoi(argv[1]);
    size = atoi(argv[2]);
    if (size > SIZE) {
	printf("%s: %s %d > %d\n", prog_name, prog_name, size, SIZE);
	exit(1);
    }
    
    printf("%s %d %d\n", prog_name, n, size);

    srandom(getpid());

    int fd;
    if((fd = creat(name, S_IRUSR | S_IWUSR)) < 0) {
		printf("main: create %s failed: %d\n", name, fd);
		exit(1);
    }

    write_test(n, size, 1);
    read_test(n, size, 1);
    write_test(n , size, 0);
    read_test(n, size, 0);
    read_test(n , size, 1);

    unlink(name);

    sync();
}


/*
 *			     Print rusage
 */

#if LINUX
print_rusage()
{
    struct rusage returnUsage;
    if (getrusage(RUSAGE_SELF, &returnUsage) != 0) return;

    printf("ru_maxrss %d ru_minflt %d ru_majflt %d ru_nswap %d ru_inblock %d "
	   "ru_oublock %d ru_msgsnd %d ru_msgrcv %d ru_nsignals %d "
	   "ru_nvcsw %d ru_nivcsw %d\n",
	   returnUsage.ru_maxrss, returnUsage.ru_minflt, returnUsage.ru_majflt,
	   returnUsage.ru_nswap, returnUsage.ru_inblock,
	   returnUsage.ru_oublock, returnUsage.ru_msgsnd,
	   returnUsage.ru_msgrcv,
	   returnUsage.ru_nsignals, returnUsage.ru_nvcsw,
	   returnUsage.ru_nivcsw);

    printf("user time %ld sec %ld usec system time %ld sec %ld usec\n",
	   returnUsage.ru_utime.tv_sec, returnUsage.ru_utime.tv_usec,
	   returnUsage.ru_stime.tv_sec, returnUsage.ru_stime.tv_usec);
}
#endif
