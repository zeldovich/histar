#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H

#include <inc/segment.h>
#include <inc/thread.h>

/* console.c */
int	iscons(int fd);
int	getchar();
int	putchar(int c);

/* readline.c */
char*	readline(const char *prompt);

/* segment.c */
int	segment_map_change(uint64_t ctemp, struct segment_map *segmap);

int	segment_alloc(uint64_t container, uint64_t bytes, struct cobj_ref *cobj);
int	segment_map(uint64_t ctemp, struct cobj_ref seg, int writable,
		    void **va_store, uint64_t *bytes_store);
int	segment_unmap(uint64_t ctemp, void *va);

/* elf.c */
int	elf_load(uint64_t container, struct cobj_ref seg, struct thread_entry *e);

#endif
