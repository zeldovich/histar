#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H

#include <inc/segment.h>
#include <inc/thread.h>
#include <inc/label.h>
#include <inc/kobj.h>
#include <inc/fs.h>

/* console.c */
int	iscons(int fd);
int	opencons(void);

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
int	segment_unmap_delayed(void *va, int can_delay);
int	segment_lookup(void *va, struct cobj_ref *seg,
		       uint64_t *npage, uint64_t *flagsp);
int	segment_lookup_obj(uint64_t oid, void **vap);

/* Notify that the thread has changed AS objects */
void	segment_as_switched(void);

/* Flush buffered unmap requests */
void	segment_unmap_flush(void);

/* For debugging purposes */
void	segment_map_print(struct u_address_space *as);

/* elf.c */
int	elf_load(uint64_t container, struct cobj_ref seg,
		 struct thread_entry *e, struct ulabel *label);

/* libmain.c */
typedef struct {
    uint64_t proc_container;
    uint64_t shared_container;
    uint64_t root_container;

    struct cobj_ref process_status_seg;
    struct cobj_ref declassify_gate;

    /* Handles that ensure process integrity */
    uint64_t process_grant;
    uint64_t process_taint;

    uint64_t user_taint;
    uint64_t user_grant;
    
    struct cobj_ref fs_mtab_seg;
    struct fs_inode fs_root;
    struct fs_inode fs_cwd;

    uint64_t ppid;

    int argc;
    int envc;
    char args[0];
} start_env_t;

extern uint64_t start_arg0, start_arg1;
extern start_env_t *start_env;

// This layout is reflected in lib/authclnt.cc and perhaps elsewhere.
extern uint64_t *tls_tidp;	/* 8 bytes for cached thread ID */
extern void *tls_gate_args;	/* struct gate_call_args */
#define TLS_GATE_ARGS	(UTLS + PGSIZE - sizeof(uint64_t) - sizeof(struct gate_call_data))
extern void *tls_stack_top;	/* same as tls_gate_args, grows down */
extern void *tls_base;		/* base */

void	libmain(uint64_t arg0, uint64_t arg1) __attribute__((__noreturn__));

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
#define PROCESS_TAINTED_EXIT	3

struct process_state {
    uint64_t status;
    int64_t exit_code;
};

struct child_process {
    uint64_t container;
    struct cobj_ref wait_seg;
};

int	process_wait(struct child_process *child, int64_t *exit_code);
int	process_report_taint(void);
int	process_report_exit(int64_t code);

/* container.c */
int64_t container_find(uint64_t container, kobject_type_t type,
		       const char *name);

/* sbrk.c */
int  heap_relabel(struct ulabel *l);

/* label.c */
typedef int (label_comparator)(level_t, level_t);
label_comparator label_leq_starlo;
label_comparator label_leq_starhi;

struct ulabel *label_alloc(void);
void label_free(struct ulabel *l);
struct ulabel *label_get_current(void);
int  label_set_level(struct ulabel *l, uint64_t handle, level_t level,
		     int grow);
level_t label_get_level(struct ulabel *l, uint64_t handle);
const char *label_to_string(const struct ulabel *l);
int  label_grow(struct ulabel *l);
int  label_compare(struct ulabel *a, struct ulabel *b, label_comparator cmp);

void label_change_star(struct ulabel *l, level_t new_level);

/* debug.cc */
void print_backtrace(void);

/* signal.c */
void signal_init(void);

#endif
