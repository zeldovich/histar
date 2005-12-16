#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/assert.h>

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
	printf("cannot get slot %ld in %ld: %s\n", slot, ct, e2s(id));
	return;
    }

    struct cobj_ref cobj = COBJ(ct, id);
    int type = sys_obj_get_type(cobj);
    if (type < 0) {
	printf("sys_obj_get_type <%ld.%ld>: %s\n",
		cobj.container, cobj.object, e2s(type));
	return;
    }

    printf("%5ld [%4ld]  ", id, slot);

    int r;
    switch (type) {
    case kobj_gate:
	printf("gate\n");
	break;

    case kobj_thread:
	printf("thread\n");
	break;

    case kobj_container:
	printf("container\n");
	break;

    case kobj_segment:
	r = sys_segment_get_npages(cobj);
	printf("segment (%d pages)\n", r);
	break;

    case kobj_address_space:
	printf("address space\n");
	break;

    default:
	printf("unknown (%d)\n", type);
    }
}

static void
builtin_list_container(int ac, char **av)
{
    if (ac != 1) {
	printf("Usage: lc <container-id>\n");
	return;
    }

    uint64_t ct = atoi(av[0]);
    printf("Container %ld:\n", ct);
    printf("   id  slot   object\n");

    int64_t nslots = sys_container_nslots(ct);
    if (nslots < 0) {
	printf("sys_container_nslots(%ld): %s\n", ct, e2s(nslots));
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
	printf("cannot get filesystem container id: %s\n", e2s(c_fs));
	return c_fs;
    }

    int64_t dir_id = sys_container_get_slot_id(c_fs, 0);
    if (dir_id < 0) {
	printf("cannot get directory segment id: %s\n", e2s(dir_id));
	return dir_id;
    }

    uint64_t *dirbuf = 0;
    int r = segment_map(COBJ(c_fs, dir_id), SEGMAP_READ,
			(void**)&dirbuf, 0);
    if (r < 0) {
	printf("cannot map dir segment <%ld.%ld>: %s\n",
		c_fs, dir_id, e2s(r));
	return r;
    }

    int max_dirent = dirbuf[0];
    int dirsize = 0;
    for (int i = 1; i <= max_dirent; i++) {
	dir[dirsize].cobj = COBJ(c_fs, dirbuf[16*i]);
	strcpy(dir[dirsize].name, (char*)&dirbuf[16*i+1]);
	//printf("readdir: %d %ld %s\n", dirsize,
	//	dir[dirsize].cobj.object, dir[dirsize].name);
	dirsize++;
    }
    //printf("readdir: done\n");

    r = segment_unmap(dirbuf);
    if (r < 0) {
	printf("cannot unmap dir segment: %s\n", e2s(r));
	return r;
    }

    return dirsize;
}

static void
builtin_ls(int ac, char **av)
{
    if (ac != 0) {
	printf("Usage: ls\n");
	return;
    }

    int r = readdir();
    if (r < 0)
	return;

    for (int i = 0; i < r; i++)
	printf("%s\n", dir[i].name);
}

static void
builtin_spawn(int ac, char **av)
{
    if (ac != 1) {
	printf("Usage: spawn <program-name>\n");
	return;
    }

    int r = readdir();
    if (r < 0)
	return;

    for (int i = 0; i < r; i++) {
	if (!strcmp(av[0], dir[i].name)) {
	    int64_t c_spawn = spawn(c_root, dir[i].cobj);
	    printf("Spawned in container %ld\n", c_spawn);
	    return;
	}
    }

    printf("Unable to find %s\n", av[0]);
}

static void
builtin_unref(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: unref <container> <object>\n");
	return;
    }

    uint64_t c = atoi(av[0]);
    uint64_t i = atoi(av[1]);

    int r = sys_obj_unref(COBJ(c, i));
    if (r < 0) {
	printf("Cannot unref <%ld:%ld>: %s\n", c, i, e2s(r));
	return;
    }

    printf("Dropped <%ld:%ld>\n", c, i);
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
    printf("Commands:\n");
    for (int i = 0; i < sizeof(commands)/sizeof(commands[0]); i++)
	printf("  %-10s %s\n", commands[i].name, commands[i].desc);
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

    printf("%s: unknown command, try help\n", av[0]);
}

int
main(int ac, char **av)
{
    c_root = start_arg;
    c_temp = start_arg;

    assert(0 == opencons(c_temp));
    assert(1 == dup(0, 1));
    assert(2 == dup(0, 2));

    printf("JOS shell (root container %ld)\n", c_root);

    for (;;) {
	char *cmd = readline("jos> ");
	parse_cmd(cmd);
	run_cmd(cmd_argc, cmd_argv);
    }
}
