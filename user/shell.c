#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>

#define MAXARGS	256
static char *cmd_argv[MAXARGS];
static int cmd_argc;

static char separators[] = " \t\n\r";
// XXX stick all our garbage in the root container...
static int ctemp = 0;

static void builtin_help(int ac, char **av);

static void
print_cobj(int type, struct cobj_ref cobj)
{
    int r;

    switch (type) {
    case cobj_gate:
	cprintf("gate\n");
	break;

    case cobj_thread:
	cprintf("thread\n");
	break;

    case cobj_container:
	r = sys_container_get_c_idx(cobj);
	cprintf("container %d\n", r);
	break;

    case cobj_segment:
	r = sys_segment_get_npages(cobj);
	cprintf("segment (%d pages)\n", r);
	break;

    default:
	cprintf("unknown (%d)\n", type);
    }
}

static void
builtin_list_container(int ac, char **av)
{
    if (ac != 1) {
	cprintf("Usage: lc <container-id>\n");
	return;
    }

    int ct = atoi(av[0]);
    cprintf("Container %d:\n", ct);

    for (int i = 0; i < 100; i++) {
	int r = sys_container_get_type(COBJ(ct, i));
	if (r < 0) {
	    cprintf("sys_container_get_type(<%d,%d>): %d\n", ct, i, r);
	    return;
	}

	if (r == cobj_none)
	    continue;
	cprintf("  %3d ", i);
	print_cobj(r, COBJ(ct, i));
    }
}

static struct {
    char name[64];
    uint64_t idx;
} dir[256];

static int
readdir()
{
    char *dirbuf;
    int r = segment_map(ctemp, COBJ(1, 0), 0, (void**)&dirbuf, 0);
    if (r < 0) {
	cprintf("cannot map dir segment: %d\n", r);
	return r;
    }

    int max_dirent = dirbuf[0];
    int dirsize = 0;
    for (int i = 1; i <= max_dirent; i++) {
	dir[dirsize].idx = dirbuf[64*i];
	strcpy(dir[dirsize].name, &dirbuf[64*i+1]);
	dirsize++;
    }

    r = segment_unmap(ctemp, dirbuf);
    if (r < 0) {
	cprintf("cannot unmap dir segment: %d\n", r);
	return r;
    }

    return dirsize;
}

static void
builtin_ls(int ac, char **av)
{
    if (ac != 0) {
	cprintf("Usage: ls\n");
	return;
    }

    int r = readdir();
    if (r < 0)
	return;

    for (int i = 0; i < r; i++)
	cprintf("[%d] %s\n", i, dir[i].name);
}

static void
builtin_spawn_seg(struct cobj_ref seg)
{
    // XXX stick the new thread in the root container
    uint64_t container = 0;

    char *segbuf;
    uint64_t bytes;
    int r = segment_map(ctemp, seg, 0, (void**)&segbuf, &bytes);
    if (r < 0) {
	cprintf("cannot map program segment: %d\n", r);
	return;
    }

    struct thread_entry e;
    memset(&e, 0, sizeof(e));
    struct segment_map *segmap = &e.te_segmap;
    int si = 0;

    Elf64_Ehdr *elf = (Elf64_Ehdr*) segbuf;
    if (elf->e_magic != ELF_MAGIC || elf->e_ident[0] != 2) {
	cprintf("ELF magic mismatch\n");
	return;
    }

    e.te_entry = (void*) elf->e_entry;
    Elf64_Phdr *ph = (Elf64_Phdr *) (segbuf + elf->e_phoff);
    for (int i = 0; i < elf->e_phnum; i++, ph++) {
	if (ph->p_type != 1)
	    continue;

	int va_off = ph->p_vaddr & 0xfff;
	struct cobj_ref seg;
	r = segment_alloc(container, va_off + ph->p_memsz, &seg);
	if (r < 0) {
	    cprintf("cannot allocate elf segment: %d\n", r);
	    return;
	}

	char *sbuf;
	r = segment_map(ctemp, seg, 1, (void**)&sbuf, 0);
	if (r < 0) {
	    cprintf("cannot map elf segment: %d\n", r);
	    return;
	}

	memcpy(sbuf + va_off, segbuf + va_off, ph->p_filesz);
	r = segment_unmap(ctemp, sbuf);
	if (r < 0) {
	    cprintf("cannot unmap elf segment: %d\n", r);
	    return;
	}

	segmap->sm_ent[si].segment = seg;
	segmap->sm_ent[si].start_page = 0;
	segmap->sm_ent[si].num_pages = (va_off + ph->p_memsz + PGSIZE - 1) / PGSIZE;
	segmap->sm_ent[si].writable = 1;
	segmap->sm_ent[si].va = (void*) (ph->p_vaddr - va_off);
	si++;
    }

    r = segment_unmap(ctemp, segbuf);
    if (r < 0) {
	cprintf("cannot unmap program segment: %d\n", r);
	return;
    }

    struct cobj_ref stack;
    r = segment_alloc(container, PGSIZE, &stack);
    if (r < 0) {
	cprintf("cannot create stack segment: %d\n", r);
	return;
    }

    char *stacktop = (char*) ULIM;
    e.te_stack = stacktop;
    segmap->sm_ent[si].segment = stack;
    segmap->sm_ent[si].start_page = 0;
    segmap->sm_ent[si].num_pages = 1;
    segmap->sm_ent[si].writable = 1;
    segmap->sm_ent[si].va = stacktop - PGSIZE;

    int thread = sys_thread_create(0);
    if (thread < 0) {
	cprintf("cannot create thread: %d\n", thread);
	return;
    }

    r = sys_thread_start(COBJ(0, thread), &e);
    if (r < 0) {
	cprintf("cannot start thread: %d\n", r);
	return;
    }

    cprintf("Running thread <0:%d>\n", thread);
}

static void
builtin_spawn(int ac, char **av)
{
    if (ac != 1) {
	cprintf("Usage: spawn <program-name>\n");
	return;
    }

    int r = readdir();
    if (r < 0)
	return;

    for (int i = 0; i < r; i++) {
	if (!strcmp(av[0], dir[i].name)) {
	    builtin_spawn_seg(COBJ(1, dir[i].idx));
	    return;
	}
    }

    cprintf("Unable to find %s\n", av[0]);
}

static void
builtin_unref(int ac, char **av)
{
    if (ac != 2) {
	cprintf("Usage: unref <container> <index>\n");
	return;
    }

    int c = atoi(av[0]);
    int i = atoi(av[1]);

    int r = sys_container_unref(COBJ(c, i));
    if (r < 0) {
	cprintf("Cannot unref <%d:%d>: %d\n", c, i, r);
	return;
    }

    cprintf("Dropped <%d:%d>\n", c, i);
}

static struct {
    const char *name;
    const char *desc;
    void (*func) (int ac, char **av);
} commands[] = {
    { "help",	"Display the list of commands",	&builtin_help },
    { "lc",	"List a container",		&builtin_list_container },
    { "ls",	"List the directory",		&builtin_ls },
    { "spawn",	"Create a thread",		&builtin_spawn },
    { "unref",	"Drop container object",	&builtin_unref },
};

static void
builtin_help(int ac, char **av)
{
    cprintf("Commands:\n");
    for (int i = 0; i < sizeof(commands)/sizeof(commands[0]); i++)
	cprintf("  %-10s %s\n", commands[i].name, commands[i].desc);
}

static void
parse_cmd(char *cmd)
{
    char *arg_base = cmd;
    char *c = cmd;

    cmd_argc = 0;
    for (;;) {
	int eoc = 0;

	if (*c == '\0')
	    eoc = 1;

	if (eoc || strchr(separators, *c)) {
	    *c = '\0';
	    if (arg_base != c)
		cmd_argv[cmd_argc++] = arg_base;
	    arg_base = c + 1;
	}

	if (eoc)
	    break;
	c++;
    }
}

static void
run_cmd(int ac, char **av)
{
    if (ac == 0)
	return;

    for (int i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
	if (!strcmp(av[0], commands[i].name)) {
	    commands[i].func(ac-1, av+1);
	    return;
	}
    }

    cprintf("%s: unknown command, try help\n", av[0]);
}

int
main(int ac, char **av)
{
    cprintf("JOS shell\n");

    for (;;) {
	char *cmd = readline("jos> ");
	parse_cmd(cmd);
	run_cmd(cmd_argc, cmd_argv);
    }
}
