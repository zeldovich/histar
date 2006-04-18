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

struct arc4 {
  u_char i;
  u_char j;
  u_char s[256];

};
typedef struct arc4 arc4;
static arc4 as;

static inline u_char
arc4_getbyte (arc4 *a)
{
  u_char si, sj;
  a->i = (a->i + 1) & 0xff;
  si = a->s[a->i];
  a->j = (a->j + si) & 0xff;
  sj = a->s[a->j];
  a->s[a->i] = sj;
  a->s[a->j] = si;
  return a->s[(si + sj) & 0xff];
}

static void
arc4_reset (arc4 *a)
{
  int n;
  a->i = 0xff;
  a->j = 0;
  for (n = 0; n < 0x100; n++)
    a->s[n] = n;
}

static void
_arc4_setkey (arc4 *a, const u_char *key, size_t keylen)
{
  u_int n, keypos;
  u_char si;
  for (n = 0, keypos = 0; n < 256; n++, keypos++) {
    if (keypos >= keylen)
      keypos = 0;
    a->i = (a->i + 1) & 0xff;
    si = a->s[a->i];
    a->j = (a->j + si + key[keypos]) & 0xff;
    a->s[a->i] = a->s[a->j];
    a->s[a->j] = si;
  }
}

static void
arc4_setkey (arc4 *a, const void *_key, size_t len)
{
  const u_char *key = (const u_char *) _key;
  arc4_reset (a);
  while (len > 128) {
    len -= 128;
    key += 128;
    _arc4_setkey (a, key, 128);
  }
  if (len > 0)
    _arc4_setkey (a, key, len);
  a->j = a->i;
}

static int
f(int i, int n)
{
    unsigned int rnd = arc4_getbyte(&as) + (arc4_getbyte(&as) << 8);
    return rnd % n;
    // return ((i * 11) % n);
}

static void
write_test(int n, int size, int sequential, int finesync)
{
    int i = 0 ;
    int r;
    int fd;
    long pos = 0 ;

    memset(buf, 0xab + sequential * 2 + finesync * 4, size);

    unsigned s, mid, fin;
    s = time_msec();
   
    int syncflag = finesync ? O_SYNC : 0;
    if ((fd = open(name, O_RDWR | syncflag, 0)) < 0) {
	printf("write_test: open %s failed: %d\n", name, fd);
	exit(1);
    }

#if LINUX
    int dir;
    if ((dir = open(".", O_RDONLY)) < 0) {
	perror("cannot open .");
	exit(1);
    }

    fsync(dir);
    close(dir);
#endif

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

		if (finesync && fsync(fd) < 0) {
		    printf("write_test: fsync failed: %s\n", strerror(errno));
		    exit(1);
		}
    }
    
    mid = time_msec();

    if (fsync(fd) < 0) {
	printf("write_test: fsync failed: %s\n", strerror(errno));
	exit(1);
    }

    if ((r = close(fd)) < 0) {
	printf("write_test: close failed %s: %d\n", name, r);
    }

    fin = time_msec();
    printf("write_test: write took %d msec (%d msec write, %d msec sync)\n", fin - s, mid-s, fin-mid);

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

int main(int argc, char *argv[])
{
    int n;
    int size;

    const char *rc4key = "hello world.";
    arc4_setkey(&as, rc4key, sizeof(rc4key));

    prog_name = argv[0];

    if (argc != 5) {
	printf("%s: %s num_blocks size_block syncopt skipopt\n", prog_name, prog_name);
	printf("  syncopt: 1 for per-file sync, 0 for group-sync\n");
	printf("  skipopt: 0 for full, 1 for write-only, 2 for read-only\n");
	exit(1);
    }

    n = atoi(argv[1]);
    size = atoi(argv[2]);
    int finesync = atoi(argv[3]);
    int skipopt = atoi(argv[4]);

    if (size > SIZE) {
	printf("%s: %s %d > %d\n", prog_name, prog_name, size, SIZE);
	exit(1);
    }
    
    printf("%s %d %d\n", prog_name, n, size);

    srandom(getpid());

    if (skipopt != 2) {
	int fd;
	if((fd = creat(name, S_IRUSR | S_IWUSR)) < 0) {
	    printf("main: create %s failed: %d\n", name, fd);
	    exit(1);
	}
    }

    if (skipopt != 2) write_test(n, size, 1, 0);
    if (skipopt != 1) read_test(n, size, 1);
    if (skipopt == 0) write_test(n, size, 0, finesync);
    if (skipopt != 1) read_test(n, size, 0);
    if (skipopt != 1) read_test(n, size, 1);

    if (!skipopt)
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
