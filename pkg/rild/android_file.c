extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <inc/stdio.h>
#include <inc/lib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/mman.h>

#include "smd_tty.h"
#include "smd_qmi.h"

#define DEBUG
#ifdef DEBUG
#define DPRINTF(_x)	cprintf _x
#else
#define DPRINTF(_x)
#endif

typedef	struct {
	unsigned long b[32];	// bionic has 32 * 32 == 1024 fds, max.
} bfd_set;

/*
 * Bionic's stdin/stdout/sterr array of FILE structs.
 * Each FILE is about 80-100 bytes.
 */
unsigned char __sF[300];

/* kludgey guess of which FILE we're talking about */
static inline FILE *
sF_to_FILE(void *p)
{
	uintptr_t pi = (uintptr_t)p;
	uintptr_t si = (uintptr_t)__sF;

	if (pi >= si && pi < (si + 80)) {
		DPRINTF(("%s: assuming %p is stdin\n", __func__, p));
		return (stdin);
	} else if (pi >= (si + 80) && pi < (si + 160)) {
		DPRINTF(("%s: assuming %p is stdout\n", __func__, p));
		return (stdout);
	} else if (pi >= (si + 160) && pi < (si + 240)) {
		DPRINTF(("%s: assuming %p is stderr\n", __func__, p));
		return (stderr);
	}
	return ((FILE *)p);
}

// our libc overrides
extern "C" {
FILE   *FOPEN(const char *, const char *);
size_t	FREAD(void *, size_t, size_t, FILE *);
size_t	FWRITE(void *, size_t, size_t, FILE *);
int	FCLOSE(FILE *);
char   *FGETS(char *, int, FILE *);
int	FILENO(FILE *);
int	FPRINTF(FILE *, const char *, ...);

int	OPEN(const char *, int, ...);
int	CLOSE(int);
ssize_t	READ(int, void *, size_t);
ssize_t	WRITE(int, const void *, size_t);
int	CHMOD(const char *, mode_t);
int	FSTAT(int, struct stat *);

#define ACOUSTIC_ARM11_DONE	0x40047016
#define ACOUSTIC_ALLOC_SMEM	0x40047017
int	IOCTL(int, unsigned long, ...);

void   *MMAP(void *, size_t, int, int, int, off_t);
int	MUNMAP(void *, size_t);

int	SOCKET(int, int, int);

int	SELECT(int, bfd_set *, bfd_set *, bfd_set *, struct timeval *);
};
// can't make these too big, else we exceed arrays somewhere (fd_set?!?)
static const int smd0_fd = 10;
static const int qmi0_fd = 11;
static const int qmi1_fd = 12;
static const int qmi2_fd = 13;
static const int acoustic_fd = 14;

#define ACOUSTIC_DEV	"/dev/fb3"

#ifdef DEBUG
static void
hexdump(const unsigned char *buf, unsigned int len)
{
	unsigned int i, j;

	i = 0;
	while (i < len) {
		char offset[9];
		char hex[16][3];
		char ascii[17];

		snprintf(offset, sizeof(offset), "%08x  ", i);
		offset[sizeof(offset) - 1] = '\0';

		for (j = 0; j < 16; j++) {
			if ((i + j) >= len) {
				strcpy(hex[j], "  ");
				ascii[j] = '\0';
			} else {
				snprintf(hex[j], sizeof(hex[0]), "%02x",
				    buf[i + j]);
				hex[j][sizeof(hex[0]) - 1] = '\0';
				if (isprint((int)buf[i + j]))
					ascii[j] = buf[i + j];
				else
					ascii[j] = '.';
			}
		}
		ascii[sizeof(ascii) - 1] = '\0';

		cprintf("%s  %s %s %s %s %s %s %s %s  %s %s %s %s %s %s %s %s  "
		    "|%s|\n", offset, hex[0], hex[1], hex[2], hex[3], hex[4],
		    hex[5], hex[6], hex[7], hex[8], hex[9], hex[10], hex[11],
		    hex[12], hex[13], hex[14], hex[15], ascii);

		i += 16;
	}
}
#endif

static int
smd_open(const char *path, int flags)
{
	DPRINTF(("SMD OPEN: path == %s, flags = 0x%x\n", path, flags));
	if (strncmp(path, "/dev/smd0", 9) == 0) {
		int r = smd_tty_open(0);
		if (r < 0)
			return (r);
		return (smd0_fd);
	}

	DPRINTF(("UNHANDLED SMD DEVICE: %s!\n", path));
	return (open(path, flags));
}

static ssize_t
smd_read(int fd, void *buf, size_t nbyte)
{
	// XXX- resid shit probably unnecessary.
	static char resid[512];
	static int  residlen = 0;

	DPRINTF(("SMD_READ: fd %d, size %d (tid %" PRIx64 ")\n", fd, nbyte, thread_id()));

	ssize_t ret;
	int do_resid = 0;

	if (residlen) {
		ret = residlen;
		residlen = 0;
		memcpy(buf, resid, ret);
		do_resid = 1;
	} else {
		ret = smd_tty_read(0, (unsigned char *)buf, nbyte);
	}

	if (ret > 0 && strncmp((const char *)buf, "+CRREADY\r\n", 10) == 0 && !do_resid) {
		residlen = ret - 10;
		ret = 10;
		if (residlen)
			memcpy(resid, (char *)buf + 10, residlen);
	}

#ifdef DEBUG
	if (ret > 0) {
		DPRINTF(("------ READ FROM SMD ------\n"));
		hexdump((const unsigned char *)buf, ret);
	}
#endif

	return (ret);
}

static ssize_t
smd_write(int fd, const void *buf, size_t nbyte)
{
	DPRINTF(("SMD_WRITE: fd %d, size %d (tid %" PRIx64 ")\n", fd, nbyte, thread_id()));

#ifdef DEBUG
	DPRINTF(("------ WRITE TO SMD ------\n"));
	hexdump((const unsigned char *)buf, nbyte);
#endif

	ssize_t ret = smd_tty_write(0, (const unsigned char *)buf, nbyte);
	DPRINTF(("SMD_WRITE: TTY WROTE %d BYTES\n", (int)ret));
	return (ret);
}

static int
qmi_fd_to_num(int fd)
{
	if (fd == qmi0_fd)
		return (0);
	if (fd == qmi1_fd)
		return (1);
	if (fd == qmi2_fd)
		return (2);

	cprintf("%s: bogus qmi fd: %d\n", __func__, fd);
	exit(1);
}

static int
qmi_open(const char *path, int flags)
{
	DPRINTF(("QMI OPEN: path == %s, flags = 0x%x\n", path, flags));
	if (strncmp(path, "/dev/qmi0", 9) == 0) {
		if (smd_qmi_open(0) < 0)
			return (-1);
		return (qmi0_fd);
	} else if (strncmp(path, "/dev/qmi1", 9) == 0) {
		if (smd_qmi_open(1) < 0)
			return (-1);
		return (qmi1_fd);
	} else if (strncmp(path, "/dev/qmi2", 9) == 0) {
		if (smd_qmi_open(2) < 0)
			return (-1);
		return (qmi2_fd);
	}

	DPRINTF(("UNHANDLED QMI DEVICE: %s!\n", path));
	return (open(path, flags));
}

static ssize_t
qmi_read(int fd, void *buf, size_t nbyte)
{
	DPRINTF(("QMI_READ: fd %d, size %d\n", fd, nbyte));

	int n = qmi_fd_to_num(fd);
	ssize_t ret = smd_qmi_read(n, (unsigned char *)buf, nbyte); 

#ifdef DEBUG
	if (ret > 0) {
		DPRINTF(("------ READ FROM QMI %d ------\n", n));
		hexdump((const unsigned char *)buf, ret);
	}
#endif

	return (ret);
}

static int
qmi_select(int nfd, bfd_set *rfds, bfd_set *wfds, bfd_set *efds, struct timeval *timeout)
{
	int ns[3] = { -1, -1, -1 };
	int rdys[3] = { 0, 0, 0 };
	int cnt = 0;

	int qmi0 = (rfds->b[qmi0_fd / 32] >> (qmi0_fd % 32)) & 0x1;
	if (qmi0) {
		DPRINTF(("%s: waiting on qmi0\n", __func__));
		ns[cnt++] = 0;
	}

	int qmi1 = (rfds->b[qmi1_fd / 32] >> (qmi1_fd % 32)) & 0x1;
	if (qmi1) {
		DPRINTF(("%s: waiting on qmi1\n", __func__));
		ns[cnt++] = 1;
	}

	int qmi2 = (rfds->b[qmi2_fd / 32] >> (qmi2_fd % 32)) & 0x1;
	if (qmi2) {
		DPRINTF(("%s: waiting on qmi2\n", __func__));
		ns[cnt++] = 2;
	}

	if (cnt == 0) {
		cprintf("%s: ERROR! CNT == 0\n", __func__);
		exit(1);
	}

	rfds->b[qmi0_fd / 32] &= ~(0x1 << (qmi0_fd % 32));
	rfds->b[qmi1_fd / 32] &= ~(0x1 << (qmi1_fd % 32));
	rfds->b[qmi2_fd / 32] &= ~(0x1 << (qmi2_fd % 32));

	smd_qmi_readwait(ns, rdys, cnt);

	int i;
	for (i = 0; i < cnt; i++) {
		if (rdys[i]) {
			int fds[3] = { qmi0_fd, qmi1_fd, qmi2_fd };
			int fd = fds[ns[i]];
			DPRINTF(("%s: qmi fd %d set\n", __func__, fd));
			rfds->b[fd / 32] |= (0x1 << (fd % 32));
		}
	}

	return (0);
}
static ssize_t
qmi_write(int fd, const void *buf, size_t nbyte)
{
	DPRINTF(("QMI_WRITE: fd %d, size %d\n", fd, nbyte));

	int n = qmi_fd_to_num(fd);

#ifdef DEBUG
	DPRINTF(("------ WRITE TO SMD %d ------\n", n));
	hexdump((const unsigned char *)buf, nbyte);
#endif

	ssize_t ret = smd_qmi_write(n, (const unsigned char *)buf, nbyte);
	DPRINTF(("QMI_WRITE: QMI WROTE %d BYTES\n", (int)ret));
	return (ret);
} 

static int
htc_acoustic_open(const char *path, int flags)
{
	DPRINTF(("HTC_ACOUSTIC OPEN: path == %s, flags = 0x%x\n", path, flags));
	return (acoustic_fd);
}

int
OPEN(const char *path, int flags, ...)
{
	if (strncmp(path, "/dev/smd", 8) == 0)
		return (smd_open(path, flags));
	else if (strncmp(path, "/dev/qmi", 8) == 0)
		return (qmi_open(path, flags));
	else if (strncmp(path, "/dev/htc-acoustic", 17) == 0)
		return (htc_acoustic_open(path, flags));

	DPRINTF(("OPEN PASS-THROUGH TO LIBC (%s)\n", path));
	return (open(path, flags));
}

int
CLOSE(int fd)
{
	DPRINTF(("CLOSE on fd %d\n", fd));
	if (fd == smd0_fd || fd == qmi0_fd || fd == qmi1_fd || fd == qmi2_fd ||
	    fd == acoustic_fd)
		return (0);

	DPRINTF(("CLOSE PASS-THROUGH TO LIBC (fd = %d)\n", fd));
	return (close(fd));
}

ssize_t
READ(int fd, void *buf, size_t nbyte)
{
	if (fd == smd0_fd)
		return (smd_read(fd, buf, nbyte));
	else if (fd == qmi0_fd || fd == qmi1_fd || fd == qmi2_fd)
		return (qmi_read(fd, buf, nbyte));

	DPRINTF(("READ PASS-THROUGH TO LIBC (fd = %d)\n", fd));
	return (read(fd, buf, nbyte));
}

ssize_t
WRITE(int fd, const void *buf, size_t nbyte)
{
	if (fd == smd0_fd)
		return (smd_write(fd, buf, nbyte));
	else if (fd == qmi0_fd || fd == qmi1_fd || fd == qmi2_fd)
		return (qmi_write(fd, buf, nbyte));

	DPRINTF(("WRITE PASS-THROUGH TO LIBC (fd = %d)\n", fd));
	return (write(fd, buf, nbyte));
}

int
IOCTL(int fd, unsigned long req, ...)
{
	DPRINTF(("IOCTL on fd %d, req %lu\n", fd, req));
	return (-1);
}

int
CHMOD(const char *path, mode_t mode)
{
	DPRINTF(("CHMOD on %s\n", path));
	return (chmod(path, mode));
}

int
FSTAT(int fd, struct stat *buf)
{
	DPRINTF(("FSTAT on fd %d\n", fd));
	return (fstat(fd, buf));
}

FILE *
FOPEN(const char *filename, const char *mode)
{
	DPRINTF(("FOPEN on [%s]\n", filename));
	return (fopen(filename, mode));
}

int
FCLOSE(FILE *fp)
{
	DPRINTF(("FCLOSE on FILE %p\n", fp));
	return (fclose(sF_to_FILE(fp)));
}

size_t
FREAD(void *p, size_t size, size_t nitems, FILE *fp)
{
	DPRINTF(("FREAD: %d items of %d bytes on FILE %p\n", nitems,
	    size, fp));
	return (fread(p, size, nitems, sF_to_FILE(fp)));
}

size_t
FWRITE(void *p, size_t size, size_t nitems, FILE *fp)
{
	DPRINTF(("FWRITE: %d items of %d bytes on FILE %p\n", nitems,
	    size, fp));
	return (fwrite(p, size, nitems, sF_to_FILE(fp)));
}

char *
FGETS(char *s, int n, FILE *fp)
{
	DPRINTF(("FGETS on FILE %p\n", fp));
	return (fgets(s, n, sF_to_FILE(fp)));
}

int
FILENO(FILE *fp)
{
	DPRINTF(("FILENO on FILE %p\n", fp));
	return (fileno(sF_to_FILE(fp)));
}

int
FPRINTF(FILE *fp, const char *fmt, ...)
{
	va_list va;

	DPRINTF(("FPRINTF on FILE %p, fmt [%s]\n", fp, fmt));
	va_start(va, fmt);
	return (vfprintf(fp, fmt, va));
	va_end(va);
}

void *
MMAP(void *ptr, size_t len, int prot, int flags, int fildes, off_t offset)
{
	DPRINTF(("MMAP at %p, fd %d, offset %u, len %u, prot 0x%x, "
	    "flags 0x%x\n", ptr, fildes, (uint32_t)offset, len, prot, flags));

	// /dev/htc-acoustic on Linux. HiStar has a /dev/fb entry.
	if (fildes == acoustic_fd) {
		DPRINTF(("  OPENING ACOUSTIC DEVICE FOR MMAP\n"));
		int fd = open(ACOUSTIC_DEV, O_RDWR);
		if (fd == -1) {
			DPRINTF(("  -- FAILED\n"));
			return (NULL);
		}
		DPRINTF(("  -- OPENED; MMAPING...\n"));
		return (mmap(ptr, len, prot, flags, fd, offset));
	}

	return (mmap(ptr, len, prot, flags, fildes, offset));
}

int
MUNMAP(void *ptr, size_t len)
{
	DPRINTF(("MUNMAP at %p, len %u\n", ptr, len));
	return (munmap(ptr, len));
}

int
SOCKET(int domain, int type, int proto)
{
	DPRINTF(("SOCKET: dom %d, type %d, proto %d\n", domain, type,
	    proto));
exit(1);
	return (0);
}

int
SELECT(int nfds, bfd_set *readfds, bfd_set *writefds, bfd_set *errorfds,
    struct timeval *timeout)
{
	DPRINTF(("SELECT: nfds %d, r %p, w %p, e %p, tout %p\n", nfds,
	    readfds, writefds, errorfds, timeout));

	int i, qmi0 = 0, qmi1 = 0, qmi2 = 0;
	for (i = 0; i < nfds; i++) {
		int rset = (readfds == NULL) ? 0 :
		    (readfds->b [i / 32] >> (i % 32)) & 0x1;
		int wset = (writefds == NULL) ? 0 :
		    (writefds->b[i / 32] >> (i % 32)) & 0x1;
		int eset = (errorfds == NULL) ? 0 :
		    (errorfds->b[i / 32] >> (i % 32)) & 0x1;

		if (!rset && !wset && !eset)
			continue;

		DPRINTF(("  fd %d waiting on %s%s%s\n", i,
		    (rset) ? "read" : "",
		    (wset) ? " write" : "",
		    (eset) ? " error" : ""));

		if (i == qmi0_fd) qmi0++;
		else if (i == qmi1_fd) qmi1++;
		else if (i == qmi2_fd) qmi2++;

		DPRINTF(("  qmi0: %d, qmi1: %d, qmi2: %d\n", qmi0, qmi1, qmi2));
	}

	if (qmi0 || qmi1 || qmi2) 
		return (qmi_select(nfds, readfds, writefds, errorfds, timeout));

	return (select(nfds, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)errorfds, timeout));
}

} // extern "C"
