#ifndef JOS_LDSO_DLSYSCALL_H
#define JOS_LDSO_DLSYSCALL_H

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define _dl_exit	exit
#define _dl_open	open
#define _dl_close	close
#define	_dl_read	read
#define _dl_write	write
#define _dl_mprotect	mprotect
#define _dl_mmap	mmap
#define _dl_munmap	munmap
#define _dl_stat	stat
#define _dl_fstat	fstat
#define _dl_getuid	getuid
#define _dl_geteuid	geteuid
#define _dl_getgid	getgid
#define _dl_getegid	getegid

#define _dl_mmap_check_error(x) (((void *) x) == MAP_FAILED)

#endif
