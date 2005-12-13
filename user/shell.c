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

static uint64_t c_root, c_temp;

static void builtin_help(int ac, char **av);

static void
print_cobj(uint64_t ct, uint64_t slot)
{
    int64_t id = sys_container_get_slot_id(ct, slot);
    if (id == -E_NOT_FOUND)
	return;
    if (id < 0) {
	cprintf("cannot get slot %ld in %ld: %s\n", slot, ct, e2s(id));
	return;
    }

    struct cobj_ref cobj = COBJ(ct, id);
    int type = sys_obj_get_type(cobj);
    if (type < 0) {
	cprintf("sys_obj_get_type <%ld.%ld>: %s\n",
		cobj.container, cobj.object, e2s(type));
	return;
    }

    cprintf("%5ld [%4ld]  ", id, slot);

    int r;
    switch (type) {
    case kobj_gate:
	cprintf("gate\n");
	break;

    case kobj_thread:
	cprintf("thread\n");
	break;

    case kobj_container:
	cprintf("container\n");
	break;

    case kobj_segment:
	r = sys_segment_get_npages(cobj);
	cprintf("segment (%d pages)\n", r);
	break;

    case kobj_address_space:
	cprintf("address space\n");
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

    uint64_t ct = atoi(av[0]);
    cprintf("Container %ld:\n", ct);
    cprintf("   id  slot   object\n");

    int64_t nslots = sys_container_nslots(ct);
    if (nslots < 0) {
	cprintf("sys_container_nslots(%ld): %s\n", ct, e2s(nslots));
	return;
    }

    for (uint64_t i = 0; i < nslots; i++)
	print_cobj(ct, i);
}

static struct {
    char name[64];
    struct cobj_ref cobj;
} dir[256];

static int
readdir(void)
{
    int64_t c_fs = sys_container_get_slot_id(c_root, 0);
    if (c_fs < 0) {
	cprintf("cannot get filesystem container id: %s\n", e2s(c_fs));
	return c_fs;
    }

    int64_t dir_id = sys_container_get_slot_id(c_fs, 0);
    if (dir_id < 0) {
	cprintf("cannot get directory segment id: %s\n", e2s(dir_id));
	return dir_id;
    }

    uint64_t *dirbuf;
    int r = segment_map(COBJ(c_fs, dir_id), SEGMAP_READ,
			(void**)&dirbuf, 0);
    if (r < 0) {
	cprintf("cannot map dir segment <%ld.%ld>: %s\n",
		c_fs, dir_id, e2s(r));
	return r;
    }

    int max_dirent = dirbuf[0];
    int dirsize = 0;
    for (int i = 1; i <= max_dirent; i++) {
	dir[dirsize].cobj = COBJ(c_fs, dirbuf[16*i]);
	strcpy(dir[dirsize].name, (char*)&dirbuf[16*i+1]);
	//cprintf("readdir: %d %ld %s\n", dirsize,
	//	dir[dirsize].cobj.object, dir[dirsize].name);
	dirsize++;
    }
    //cprintf("readdir: done\n");

    r = segment_unmap(dirbuf);
    if (r < 0) {
	cprintf("cannot unmap dir segment: %s\n", e2s(r));
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
	cprintf("%s\n", dir[i].name);
}

static void
builtin_spawn_seg(struct cobj_ref seg)
{
    int64_t c_spawn = sys_container_alloc(c_root);
    if (c_spawn < 0) {
	cprintf("cannot allocate container for new thread: %s\n",
		e2s(c_spawn));
	return;
    }

    // XXX these things are ridiculously huge
    static struct thread_entry e;

    int r = elf_load(c_spawn, seg, &e);
    if (r < 0) {
	cprintf("cannot load ELF: %s\n", e2s(r));
	return;
    }

    int64_t thread = sys_thread_create(c_spawn);
    if (thread < 0) {
	cprintf("cannot create thread: %s\n", e2s(thread));
	return;
    }

    // Pass the thread's container as the argument
    e.te_arg = c_spawn;

    r = sys_thread_start(COBJ(c_spawn, thread), &e);
    if (r < 0) {
	cprintf("cannot start thread: %s\n", e2s(r));
	return;
    }

    cprintf("Running thread <%ld:%ld>\n", c_spawn, thread);
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
	cprintf("Usage: unref <container> <object>\n");
	return;
    }

    uint64_t c = atoi(av[0]);
    uint64_t i = atoi(av[1]);

    int r = sys_obj_unref(COBJ(c, i));
    if (r < 0) {
	cprintf("Cannot unref <%ld:%ld>: %s\n", c, i, e2s(r));
	return;
    }

    cprintf("Dropped <%ld:%ld>\n", c, i);
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
