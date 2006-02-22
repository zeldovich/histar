// LFS large file benchmark

#define JOS64 1
#define LINUX 0

#if JOS64
#include <inc/string.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <user/performance_wrap.ss>
#endif

#if LINUX
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include <sys/timeb.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#define SIZE	8192

static char buf[SIZE];
static char name[32] = "test_file";
static char *prog_name;
static int fd;

extern int errno;


#define TRULY_RANDOM


int f(int i, int n)
{
    return ((i * 11) % n);
}

void
write_test(int n, int size, int sequential)
{
    int i = 0 ;
    int r;
    int fd;
    long pos = 0 ;
	struct stat statb;
    
    unsigned s, fin;
    s = time(0);
    
    if((fd = open(name, O_RDWR, 0)) < 0) {
	printf("%s: open %d failed %d %d\n", prog_name, i, fd, errno);
	exit(1);
    }

    for (i = 0; i < n; i ++) {
	if (!sequential) {

#ifdef TRULY_RANDOM
	    pos = (random() % n) * size;
#else
	    pos = f(i, n) * size;
#endif

	    if ((r = lseek(fd, pos, 0)) < 0) {
		printf("%s: lseek failed %d %d\n", prog_name, r, errno);
	    }
	}

	if ((r = write(fd, buf, size)) < 0) {
	    printf("%s: write failed %d %d (%ld)\n", prog_name, r, errno,
		   pos);
	    exit(1);
	}
    }
    
    fsync(fd);
    fstat(fd, &statb);
    if (fchown(fd, statb.st_uid, -1) < 0) {
	perror("fchown");
    }

    if ((r = close(fd)) < 0) {
	printf("%s: close failed %d %d\n", prog_name, r, errno);
    }

    fin = time(0);
    printf("%s: write took %d sec\n", prog_name, fin - s);

}


int g(int i, int n)
{
    if (i % 2 == 0) return(n / 2 + i / 2);
    else return(i / 2);
}


void
read_test(int n, int size, int sequential)
{
    int i = 0 ;
    int r;
    int fd;
    long pos;


    unsigned s, fin;
    s = time(0);
    
    if((fd = open(name, O_RDONLY, 0)) < 0) {
	printf("%s: open %d failed %d %d\n", prog_name, i, fd, errno);
	exit(1);
    }

    for (i = 0; i < n; i ++) {
	if (!sequential) {

#ifdef TRULY_RANDOM
	    pos = (random() % n) * size;
#else
	    pos = g(i, n) * size;
#endif

	    if ((r = lseek(fd, pos, 0)) < 0) {
		printf("%s: lseek failed %d %d\n", prog_name, r, errno);
	    }
	}

	if ((r = read(fd, buf, size)) < 0) {
	    printf("%s: read failed %d %d\n", prog_name, r, errno);
	    exit(1);
	}
    }
    
    if ((r = close(fd)) < 0) {
	printf("%s: close failed %d %d\n", prog_name, r, errno);
    }
    

    fin = time(0);
    printf("%s: read took %d sec\n",
	   prog_name,
	   fin - s);
}


void
flush_cache()
{
    int i = 0 ;
    int r = 0 ;

    if((fd = open("t", O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) < 0) {
	printf("%s: create %d failed %d %d\n", prog_name, i, fd, errno);
	exit(1);
    }

    for (i = 0; i < 15000; i ++) {
	if ((r = write(fd, buf, 4096)) < 0) {
	    printf("%s: write failed %d %d\n", prog_name, r, errno);
	    exit(1);
	}
    }
    
    fsync(fd);

    if ((r = close(fd)) < 0) {
	printf("%s: mnx_close failed %d %d\n", prog_name, r, errno);
    }

    unlink("t");
}



int main(int argc, char *argv[])
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

    srandom(getpid());

    if((fd = creat(name, S_IRUSR | S_IWUSR)) < 0) {
	printf("%s: create %d failed %d\n", prog_name, fd, errno);
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
