#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>

#define MAXARGS	256
static char *cmd_argv[MAXARGS];
static int cmd_argc;

static char separators[] = " \t\n\r";

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

    case cobj_pmap:
	cprintf("pmap\n");
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

    int i;
    for (i = 0; i < 100; i++) {
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
    const char *name;
    const char *desc;
    void (*func) (int ac, char **av);
} commands[] = {
    { "help",	"Display the list of commands",	&builtin_help },
    { "lc",	"List a container",		&builtin_list_container },
};

static void
builtin_help(int ac, char **av)
{
    int i;

    cprintf("Commands:\n");
    for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++)
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

    int i;
    for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
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
