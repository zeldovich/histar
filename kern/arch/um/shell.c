#include <stdio.h>
#include <stdlib.h>
#include <machine/um.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/sched.h>
#include <kern/pstate.h>
#include <kern/prof.h>
#include <kern/timer.h>
#include <kern/handle.h>
#include <inc/error.h>

struct cmd {
    const char *cmd;
    void (*func)(char *);
};

static char whitespace[] = " \t\r\n\v";

static char *
gettok(char **ps)
{
    char *s = *ps;
    while (strchr(whitespace, *s))
	s++;

    char *t = s;
    while (*s && !strchr(whitespace, *s))
	s++;

    if (*s) {
	*s = '\0';
	s++;
    }

    *ps = s + 1;
    return *t ? t : 0;
}

static void
cmd_load(char *p)
{
    int r = pstate_load();
    printf("pstate_load: %s\n", e2s(r));
}

static void
cmd_bench(char *p)
{
    um_bench();
}

static void cmd_cmds(char *p);
static struct cmd cmds[] = {
    { "cmds", &cmd_cmds },
    { "load", &cmd_load },
    { "bench", &cmd_bench },
};

static void
cmd_cmds(char *p)
{
    for (uint32_t i = 0; i < sizeof(cmds) / sizeof(*cmds); i++)
	printf("  %s\n", cmds[i].cmd);
}

void
um_shell(void)
{
    char buf[1024];
    printf("um> ");
    if (fgets(buf, sizeof(buf), stdin) == 0)
	exit(0);

    char *p = &buf[0];
    char *cmd = gettok(&p);
    for (uint32_t i = 0; i < sizeof(cmds) / sizeof(*cmds); i++) {
	if (cmd && !strcmp(cmd, cmds[i].cmd)) {
	    cmds[i].func(p);
	    return;
	}
    }

    printf("unknown command %s, try 'cmds'\n", cmd);
}
