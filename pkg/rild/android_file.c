extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <inc/stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/mman.h>

#include "smd_tty.h"

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
int	IOCTL(int, unsigned long, ...);
int	FSTAT(int, struct stat *);

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
	DPRINTF(("SMD_READ: fd %d, size %d\n", fd, nbyte));

	ssize_t ret = smd_tty_read(0, (unsigned char *)buf, nbyte);

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
	DPRINTF(("SMD_WRITE: fd %d, size %d\n", fd, nbyte));

#ifdef DEBUG
	DPRINTF(("------ WRITE TO SMD ------\n"));
	hexdump((const unsigned char *)buf, nbyte);
#endif

	ssize_t ret = smd_tty_write(0, (const unsigned char *)buf, nbyte);
	DPRINTF(("SMD_WRITE: TTY WROTE %d BYTES\n", (int)ret));
	return (ret);
} 

static int
qmi_open(const char *path, int flags)
{
	DPRINTF(("QMI OPEN: path == %s, flags = 0x%x\n", path, flags));
	if (strncmp(path, "/dev/qmi0", 9) == 0)
		return (qmi0_fd);
	else if (strncmp(path, "/dev/qmi1", 9) == 0)
		return (qmi1_fd);
	else if (strncmp(path, "/dev/qmi2", 9) == 0)
		return (qmi2_fd);

	DPRINTF(("UNHANDLED SMD DEVICE: %s!\n", path));
	return (open(path, flags));
}

static ssize_t
qmi_read(int fd, void *buf, size_t nbyte)
{
	DPRINTF(("QMI_READ: fd %d, size %d\n", fd, nbyte));
	return (-1);
}

static ssize_t
qmi_write(int fd, const void *buf, size_t nbyte)
{
	DPRINTF(("QMI_WRITE: fd %d, size %d\n", fd, nbyte));
	return (nbyte);
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
	DPRINTF(("IOCTL on fd %d\n", fd));
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
// XXXXXXXXX
sleep(9999);
	int i;
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

	}
	return (select(nfds, (fd_set *)readfds, (fd_set *)writefds, (fd_set *)errorfds, timeout));
}

} // extern "C"