#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H

#include <inc/segment.h>
#include <inc/thread.h>
#include <inc/label.h>
#include <inc/kobj.h>
#include <inc/fs.h>

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
		      struct cobj_ref *cobj, void **va_p,
		      struct ulabel *label, const char *name);
int	segment_map_as(struct cobj_ref as, struct cobj_ref seg,
		       uint64_t flags, void **vap, uint64_t *bytesp);
int	segment_map(struct cobj_ref seg, uint64_t flags,
		    void **va_p, uint64_t *bytes_store);
int	segment_unmap(void *va);
int	segment_lookup(void *va, struct cobj_ref *seg, uint64_t *npage);
int	segment_lookup_obj(uint64_t oid, void **vap);
void	segment_set_default_label(struct ulabel *l);
struct ulabel *segment_get_default_label(void);

/* elf.c */
int	elf_load(uint64_t container, struct cobj_ref seg,
		 struct thread_entry *e, struct ulabel *label);

/* libmain.c */
typedef struct {
    uint64_t container;
    uint64_t root_container;

    struct cobj_ref taint_mlt;

    struct fs_mount_table fs_mtab;
    struct fs_inode fs_root;
    struct fs_inode fs_cwd;

    char args[0];
} start_env_t;

extern uint64_t start_arg0, start_arg1;
extern start_env_t *start_env;

void	libmain(uint64_t arg0, uint64_t arg1) __attribute__((__noreturn__));
void    exit(int status) __attribute__((__noreturn__));

/* thread.c */
struct thread_args {
    struct cobj_ref container;
    void *stackbase;

    void (*entry)(void *);
    void *arg;
};

int	thread_create(uint64_t container, void (*entry)(void*),
		      void *arg, struct cobj_ref *threadp, const char *name);
uint64_t thread_id(void);
void	thread_halt(void) __attribute__((noreturn));
int	thread_get_label(struct ulabel *ul);
void	thread_sleep(uint64_t msec);

/* spawn.cc */
#define	PROCESS_RUNNING		0
#define PROCESS_TAINTED		1
#define PROCESS_EXITED		2

struct process_state {
    uint64_t status;
    int64_t exit_code;
};

int	process_wait(uint64_t childct, int64_t *exit_code);
int	process_report_taint(void);
int	process_report_exit(int64_t code);

/* container.c */
int64_t container_find(uint64_t container, kobject_type_t type,
		       const char *name);

/* heap.c */
void *sbrk(intptr_t x);
int  heap_relabel(struct ulabel *l);

/* malloc.c */
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);

/* label.c */
typedef int (label_comparator)(level_t, level_t);
label_comparator label_leq_starlo;
label_comparator label_leq_starhi;

struct ulabel *label_alloc(void);
void label_free(struct ulabel *l);
struct ulabel *label_get_current(void);
struct ulabel *label_get_obj(struct cobj_ref o);
int  label_set_current(struct ulabel *l);
int  label_set_level(struct ulabel *l, uint64_t handle, level_t level,
		     bool_t grow);
level_t label_get_level(struct ulabel *l, uint64_t handle);
const char *label_to_string(const struct ulabel *l);
int  label_grow(struct ulabel *l);
struct ulabel *label_dup(struct ulabel *l);
int  label_compare(struct ulabel *a, struct ulabel *b, label_comparator cmp);

// for all i, if l->ul_ent[i] < l->ul_default then l->ul_ent[i] := l->ul_default
void label_max_default(struct ulabel *l);
void label_change_star(struct ulabel *l, level_t new_level);

/* file.c */
int  open(const char *pn, int flags, int mode);
int  mkdir(const char *pn, int mode);
int  unlink(const char *pn);

/* debug.cc */
void print_backtrace(void);

#endif
