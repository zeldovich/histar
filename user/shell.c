#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>

#define MAXARGS	256
static char *cmd_argv[MAXARGS];
static int cmd_argc;

static char separators[] = " \t\n\r";

static int c_root, c_temp;

static void builtin_help(int ac, char **av);

static void
print_cobj(struct cobj_ref cobj)
{
    int type = sys_obj_get_type(cobj);
    if (type == -E_NOT_FOUND)
	return;

    if (type < 0) {
	cprintf("sys_obj_get_type <%ld.%ld>: %d\n", cobj.container, cobj.slot, type);
	return;
    }

    int id = sys_obj_get_id(cobj);
    if (id < 0) {
	cprintf("sys_obj_get_id <%ld.%ld>: %d\n", cobj.container, cobj.slot, id);
	return;
    }

    cprintf("%4ld %4d  ", cobj.slot, id);

    int r;
    switch (type) {
    case kobj_gate:
	cprintf("gate\n");
	break;

    case kobj_thread:
	cprintf("thread\n");
	break;

    case kobj_container:
	cprintf("container\n", r);
	break;

    case kobj_segment:
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
    cprintf("slot   id  object\n");

    int nslots = sys_container_nslots(ct);
    if (nslots < 0) {
	cprintf("sys_container_nslots(%d): %d\n", ct, nslots);
	return;
    }

    for (int i = 0; i < nslots; i++)
	print_cobj(COBJ(ct, i));
}

static struct {
    char name[64];
    struct cobj_ref cobj;
} dir[256];

static int
readdir()
{
    int64_t c_fs = sys_obj_get_id(COBJ(c_root, 0));
    if (c_fs < 0) {
	cprintf("cannot get filesystem container id: %ld\n", c_fs);
	return c_fs;
    }

    char *dirbuf;
    int r = segment_map(c_temp, COBJ(c_fs, 0), 0, (void**)&dirbuf, 0);
    if (r < 0) {
	cprintf("cannot map dir segment: %d\n", r);
	return r;
    }

    int max_dirent = dirbuf[0];
    int dirsize = 0;
    for (int i = 1; i <= max_dirent; i++) {
	dir[dirsize].cobj = COBJ(c_fs, dirbuf[64*i]);
	strcpy(dir[dirsize].name, &dirbuf[64*i+1]);
	dirsize++;
    }

    r = segment_unmap(c_temp, dirbuf);
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
    int c_spawn_slot = sys_container_alloc(c_root);
    if (c_spawn_slot < 0) {
	cprintf("cannot allocate container for new thread: %d\n", c_spawn_slot);
	return;
    }

    int64_t c_spawn = sys_obj_get_id(COBJ(c_root, c_spawn_slot));
    if (c_spawn < 0) {
	cprintf("cannot get new container ID: %d\n", c_spawn);
	return;
    }

    // XXX these things are ridiculously huge
    static struct thread_entry e;

    int r = elf_load(c_spawn, seg, &e);
    if (r < 0) {
	cprintf("cannot load ELF: %d\n", r);
	return;
    }

    int thread = sys_thread_create(c_spawn);
    if (thread < 0) {
	cprintf("cannot create thread: %d\n", thread);
	return;
    }

    r = sys_thread_start(COBJ(c_spawn, thread), &e);
    if (r < 0) {
	cprintf("cannot start thread: %d\n", r);
	return;
    }

    cprintf("Running thread <%d:%d>\n", c_spawn, thread);
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
	    builtin_spawn_seg(dir[i].cobj);
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

    int r = sys_obj_unref(COBJ(c, i));
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
    c_root = start_arg;
    c_temp = start_arg;

    cprintf("JOS shell (root container %ld)\n", c_root);

    for (;;) {
	char *cmd = readline("jos> ");
	parse_cmd(cmd);
	run_cmd(cmd_argc, cmd_argv);
    }
}
