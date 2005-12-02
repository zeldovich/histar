#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H

#include <inc/segment.h>

/* console.c */
int	iscons(int fd);
int	getchar();
int	putchar(int c);

/* readline.c */
char*	readline(const char *prompt);

/* segment.c */
int	segment_map_change(uint64_t ctemp, struct segment_map *segmap);
int	segment_map(uint64_t ctemp, struct cobj_ref seg, int writable,
		    void **va_store, uint64_t *bytes_store);
int	segment_unmap(uint64_t ctemp, void *va);

#endif
