#ifndef JOS_INC_LIB_H
#define JOS_INC_LIB_H

#include <inc/segment.h>
#include <inc/thread.h>
#include <inc/label.h>
#include <inc/kobj.h>
#include <inc/fs.h>
#include <inc/intmacro.h>

#define NSEC_PER_SECOND		UINT64(1000000000)

/* console.c */
int	iscons(int fd);
int	opencons(void);

/* readline.c */
char*	readline(const char *prompt);

/* segment.c */
#define SEG_MAPOPT_REPLACE	0x01
#define SEG_MAPOPT_OVERLAP	0x02

int	segment_alloc(uint64_t container, uint64_t bytes,
		      struct cobj_ref *cobj, void **va_p,
		      const struct ulabel *label, const char *name);
int	segment_map_as(struct cobj_ref as, struct cobj_ref seg,
		       uint64_t start_byteoff, uint64_t flags,
		       void **vap, uint64_t *bytesp, uint64_t map_opts);
int	segment_map(struct cobj_ref seg,
		    uint64_t start_byteoff, uint64_t flags,
		    void **va_p, uint64_t *bytes_store,
		    uint64_t map_opts);
int	segment_unmap(void *va);
int	segment_unmap_delayed(void *va, int can_delay);
int	segment_unmap_kslot(uint32_t kslot, int can_delay);
int	segment_unmap_range(void *va_start, void *va_end, int can_delay);
int	segment_lookup(void *va, struct u_segment_mapping *usm);
int	segment_lookup_skip(void *va, struct u_segment_mapping *usm,
			    uint64_t skip_flags);
int	segment_lookup_obj(uint64_t oid, struct u_segment_mapping *usm);
int	segment_set_utrap(void *entry, void *stack_base, void *stack_top);

/* Notify that the thread has changed AS objects */
void	segment_as_switched(void);
void	segment_as_invalidate_nowb(void);

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
    struct cobj_ref process_gid_seg;
    struct cobj_ref declassify_gate;

    /* Handles that ensure process integrity */
    uint64_t process_grant;
    uint64_t process_taint;

    uint64_t user_taint;
    uint64_t user_grant;

    struct cobj_ref time_seg;
    struct cobj_ref taint_cow_as;
    struct cobj_ref fs_mtab_seg;
    struct fs_inode fs_root;
    struct fs_inode fs_cwd;

    uint64_t ppid;
    char trace_on;
    int ruid, euid;

    int argc;
    int envc;
    char args[];
} start_env_t;

extern uint64_t start_arg0, start_arg1;
extern start_env_t *start_env;
extern int setup_env_done;
extern const char *jos_progname;

extern void *tls_top;
extern uint64_t *tls_tidp;	/* 8 bytes for cached thread ID */
extern struct jos_jmp_buf **tls_pgfault;
extern struct jos_jmp_buf **tls_pgfault_all;
extern void *tls_gate_args;	/* struct gate_call_args */
#define TLS_GATE_ARGS	(UTLSTOP - sizeof(uint64_t) - sizeof(*tls_pgfault) - sizeof(*tls_pgfault_all) - sizeof(struct gate_call_data))
extern void *tls_stack_top;	/* same as tls_gate_args, grows down */
extern void *tls_base;		/* base */

void	libmain(uintptr_t bootstrap, uintptr_t arg0, uintptr_t arg1)
    __attribute__((__noreturn__));
void    setup_env(uintptr_t bootstrap, uintptr_t arg0, uintptr_t arg1);
void	tls_revalidate(void);

/* thread.c */
struct thread_args {
    uint64_t container;
    uint64_t thread_id;
    uint64_t stack_id;
    void *stackbase;

    void (*entry)(void *);
    void *arg;
    int options;
    char entry_args[];
};

enum { thread_quota_slush = 65536 };
enum { thread_stack_pages = 2560 };	/* 10MB max stack */

#define THREAD_OPT_ARGCOPY	0x02

int	thread_create(uint64_t container, void (*entry)(void*),
		      void *arg, struct cobj_ref *threadp, const char *name);
int     thread_create_option(uint64_t container, void (*entry)(void*),
			     void *arg, uint32_t size_arg,
			     struct cobj_ref *threadp, const char *name, 
			     struct thread_args *thargs, int options);
uint64_t thread_id(void);
void	thread_halt(void) __attribute__((noreturn));
int	thread_get_label(struct ulabel *ul);
void	thread_sleep_nsec(uint64_t nsec);
int     thread_cleanup(struct thread_args *ta);

/* spawn.cc */
#define	PROCESS_RUNNING		0
#define PROCESS_TAINTED		1
#define PROCESS_EXITED		2
#define PROCESS_TAINTED_EXIT	3

struct process_state {
    uint64_t status;
    int64_t exit_code;
    int64_t exit_signal;
};

struct child_process {
    uint64_t container;
    struct cobj_ref wait_seg;
};

int	process_wait(const struct child_process *child, int64_t *exit_code);
int	process_report_taint(void);
int	process_report_exit(int64_t code, int64_t signo);
void	process_exit(int64_t rval, int64_t signo) __attribute__((noreturn));

/* container.c */
int64_t container_find(uint64_t container, kobject_type_t type,
		       const char *name);

/* label.c */
typedef int (label_comparator)(level_t, level_t);
label_comparator label_leq_starlo;
label_comparator label_leq_starhi;

struct ulabel *label_alloc(void);
void label_free(struct ulabel *l);
int  label_set_level(struct ulabel *l, uint64_t handle, level_t level,
		     int grow);
level_t label_get_level(struct ulabel *l, uint64_t handle);
const char *label_to_string(const struct ulabel *l);
int  label_grow(struct ulabel *l);
int  label_compare(struct ulabel *a, struct ulabel *b, label_comparator cmp);

void label_change_star(struct ulabel *l, level_t new_level);

/* debug.cc */
void print_backtrace(int use_cprintf);

/* signal.c */
void signal_init(void);

#endif
