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
#include <kern/uinit.h>
#include <kern/disk.h>
#include <inc/error.h>
#include <inc/string.h>

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

    *ps = s;
    return *t ? t : 0;
}

static uint64_t cur_id;

#define retry(expr)						\
    ({								\
	int __r;						\
	for (;;) {						\
	    __r = (expr);					\
	    if (__r != -E_RESTART)				\
		break;						\
	    pstate_op_check();					\
	    disk_poll(pstate_part->pd_dk);			\
	}							\
	__r;							\
    })

#define check(expr)						\
    do {							\
	int __c = retry(expr);					\
	if (__c < 0) {						\
	    printf("%s:%d: %s: %s\n", __FILE__, __LINE__,	\
		   #expr, e2s(__c));				\
	    return;						\
	}							\
    } while (0)

static void
cmd_load(char *p)
{
    static int loaded;

    if (loaded++) {
	printf("already loaded\n");
	return;
    }

    int r = pstate_load();
    printf("pstate_load: %d (%s)\n", r, e2s(r));

    if (r > 0) {
	cur_id = user_root_ct;
	printf("changing current object to root (%"PRIu64")\n", cur_id);
    }
}

static const char *
type_to_string(uint32_t type)
{
    switch (type) {
    case kobj_container:	return "container";
    case kobj_thread:		return "thread";
    case kobj_gate:		return "gate";
    case kobj_segment:		return "segment";
    case kobj_address_space:	return "as";
    case kobj_device:		return "device";
    case kobj_label:		return "label";
    case kobj_dead:		return "dead";
    default:			return "unknown";
    }
}

static void
cmd_ls(char *p)
{
    printf("object id: %"PRIu64"\n", cur_id);

    const struct kobject *ko;
    check(kobject_get(cur_id, &ko, kobj_any, iflow_none));

    printf("name:      %s\n", ko->hdr.ko_name);
    printf("type:      %s\n", type_to_string(ko->hdr.ko_type));

    switch (ko->hdr.ko_type) {
    case kobj_container:
	for (uint32_t i = 0; i < container_nslots(&ko->ct); i++) {
	    uint64_t id;
	    int r = retry(container_get(&ko->ct, &id, i));
	    if (r < 0)
		continue;

	    const struct kobject *cko;
	    r = retry(kobject_get(id, &cko, kobj_any, iflow_none));
	    if (r < 0)
		continue;

	    printf("  slot %d: %s %"PRIu64" (%s)\n",
		   i, type_to_string(cko->hdr.ko_type),
		   cko->hdr.ko_id, cko->hdr.ko_name);
	}
	break;

    default:
	break;
    }
}

static void
cmd_cd(char *p)
{
    char *arg = gettok(&p);
    if (!arg) {
	printf("usage: cd name-or-id\n");
	return;
    }

    uint64_t id;
    int r = strtou64(arg, 0, 10, &id);
    if (r >= 0) {
	cur_id = id;
	return;
    }

    const struct kobject *ko;
    check(kobject_get(cur_id, &ko, kobj_any, iflow_none));

    if (!strcmp(arg, "/")) {
	cur_id = user_root_ct;
	return;
    }

    if (!strcmp(arg, "..")) {
	cur_id = ko->hdr.ko_parent;
	return;
    }

    if (ko->hdr.ko_type != kobj_container) {
	printf("cannot cd inside a non-container\n");
	return;
    }

    for (uint32_t i = 0; i < container_nslots(&ko->ct); i++) {
	r = retry(container_get(&ko->ct, &id, i));
	if (r < 0)
	    continue;

	const struct kobject *cko;
	r = retry(kobject_get(id, &cko, kobj_any, iflow_none));
	if (r < 0)
	    continue;

	if (!strcmp(arg, cko->hdr.ko_name)) {
	    cur_id = cko->hdr.ko_id;
	    return;
	}
    }

    printf("%s: not found\n", arg);
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
    { "ls", &cmd_ls },
    { "cd", &cmd_cd },
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
