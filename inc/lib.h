#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H

#include <inc/segment.h>
#include <inc/thread.h>
#include <inc/label.h>

#include <lwip/inet.h>
#include <lwip/sockets.h>

/* console.c */
int	iscons(int fd);
int	getchar(void);
int	putchar(int c);
int	opencons(uint64_t container);

/* readline.c */
char*	readline(const char *prompt);

/* segment.c */
int	segment_alloc(uint64_t container, uint64_t bytes,
		      struct cobj_ref *cobj, void **va_p);
int	segment_map_as(struct cobj_ref as, struct cobj_ref seg,
		       uint64_t flags, void **vap, uint64_t *bytesp);
int	segment_map(struct cobj_ref seg, uint64_t flags,
		    void **va_p, uint64_t *bytes_store);
int	segment_unmap(void *va);
int	segment_lookup(void *va, struct cobj_ref *seg, uint64_t *npage);

/* elf.c */
int	elf_load(uint64_t container, struct cobj_ref seg,
		 struct thread_entry *e);

/* libmain.c */
extern uint64_t start_arg, start_arg1;
void	libmain(uint64_t arg0, uint64_t arg1) __attribute__((__noreturn__));

/* printfmt.c */
const char *e2s(int err);

/* thread.c */
int	thread_create(uint64_t container, void (*entry)(void*), void *arg, struct cobj_ref *threadp);
uint64_t thread_id(void);
void	thread_halt(void) __attribute__((noreturn));
int	thread_get_label(uint64_t ctemp, struct ulabel *ul);

/* fd.c */
ssize_t	read(int fd, void *buf, size_t nbytes);
ssize_t	write(int fd, const void *buf, size_t nbytes);
int	seek(int fd, off_t offset);
int	dup_as(int oldfd, int newfd, struct cobj_ref target_as);
int	dup(int oldfd, int newfd);
int	close(int fd);

int	bind(int fd, struct sockaddr *addr, socklen_t addrlen);
int	listen(int fd, int backlog);
int	accept(int fd, struct sockaddr *addr, socklen_t *addrlen);

void	close_all(void);
ssize_t	readn(int fd, void *buf, size_t nbytes);

/* spawn.c */
int64_t spawn(uint64_t container, struct cobj_ref elf);
int64_t spawn_fd(uint64_t container, struct cobj_ref elf,
		 int fd0, int fd1, int fd2);

#endif
