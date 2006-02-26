#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/elf64.h>
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/fs.h>
#include <inc/fd.h>

#define MAXARGS	256
static char *cmd_argv[MAXARGS];
static int cmd_argc;

static int label_debug = 0;

static char separators[] = " \t\n\r";

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
	printf("gate");
	break;

    case kobj_thread:
	printf("thread");
	break;

    case kobj_container:
	printf("container");
	break;

    case kobj_segment:
	printf("segment");
	r = sys_segment_get_nbytes(cobj);
	if (r < 0)
	    printf(" (cannot get bytes: %s)", e2s(r));
	else
	    printf(" (%d bytes)", r);
	break;

    case kobj_address_space:
	printf("address space");
	break;

    case kobj_mlt:
	printf("mlt");
	break;

    case kobj_netdev:
	printf("netdev");
	break;

    default:
	printf("unknown (%d)", type);
    }

    char name[KOBJ_NAME_LEN];
    r = sys_obj_get_name(cobj, &name[0]);
    if (r < 0) {
	printf(" (cannot get name: %s)\n", e2s(r));
    } else if (name[0]) {
	printf(": %s\n", name);
    } else {
	printf("\n");
    }
}

static void
builtin_list_container(int ac, char **av)
{
    if (ac != 1) {
	printf("Usage: lc <container-id>\n");
	return;
    }

    uint64_t ct;
    int r = strtoull(av[0], 0, 10, &ct);
    if (r < 0) {
	printf("bad number: %s\n", av[0]);
	return;
    }

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

static int64_t
do_spawn(int ac, char **av)
{
    const char *pn = av[0];
    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r < 0 && pn[0] != '/') {
	char buf[512];
	snprintf(buf, sizeof(buf), "/bin/%s", pn);
	r = fs_namei(buf, &ino);
    }

    if (r < 0) {
	printf("cannot find %s: %s\n", pn, e2s(r));
	return r;
    }

    struct ulabel *label = label_get_current();
    if (label == 0) {
	printf("cannot get label: out of memory?\n");
	return -E_NO_MEM;
    }

    label_change_star(label, label->ul_default);

    if (label_debug)
	printf("shell: spawning with label %s\n",
	       label_to_string(label));

    int64_t c_spawn = spawn(start_env->container, ino,
			    0, 1, 2,
			    ac, (const char **) av,
			    label, label, 0);
    label_free(label);
    if (c_spawn < 0)
	printf("cannot spawn %s: %s\n", pn, e2s(c_spawn));

    return c_spawn;
}

static void
builtin_spawn(int ac, char **av)
{
    if (ac < 1) {
	printf("Usage: spawn <program-name>\n");
	return;
    }

    int64_t ct = do_spawn(ac, av);
    if (ct >= 0)
	printf("Spawned in container %ld\n", ct);
}

static void
spawn_and_wait(int ac, char **av)
{
    int64_t ct = do_spawn(ac, av);
    if (ct < 0)
	return;

    int r = spawn_wait(ct);
    if (r < 0) {
	printf("spawn_wait: %s\n", e2s(r));
	return;
    }

    sys_obj_unref(COBJ(start_env->container, ct));
}

static void
builtin_unref(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: unref <container> <object>\n");
	return;
    }

    uint64_t c, i;

    int r = strtoull(av[0], 0, 10, &c);
    if (r < 0) {
	printf("bad number: %s\n", av[0]);
	return;
    }

    r = strtoull(av[1], 0, 10, &i);
    if (r < 0) {
	printf("bad number: %s\n", av[1]);
	return;
    }

    r = sys_obj_unref(COBJ(c, i));
    if (r < 0) {
	printf("Cannot unref <%ld.%ld>: %s\n", c, i, e2s(r));
	return;
    }

    printf("Dropped <%ld.%ld>\n", c, i);
}

static void
builtin_cd(int ac, char **av)
{
    if (ac != 1) {
	printf("cd <pathname>\n");
	return;
    }

    const char *pn = av[0];
    struct fs_inode dir;
    int r = fs_namei(pn, &dir);
    if (r < 0) {
	printf("cannot cd to %s: %s\n", pn, e2s(r));
	return;
    }

    start_env->fs_cwd = dir;
}

static void
builtin_mount(int ac, char **av)
{
    if (ac == 0) {
	for (int i = 0; i < FS_NMOUNT; i++) {
	    struct fs_mtab_ent *mtab = &start_env->fs_mtab.mtab_ent[i];
	    if (mtab->mnt_name[0])
		printf("<%ld.%ld>: %s -> <%ld.%ld>\n",
		       mtab->mnt_dir.obj.container, mtab->mnt_dir.obj.object,
		       mtab->mnt_name,
		       mtab->mnt_root.obj.container, mtab->mnt_root.obj.object);
	}
    } else if (ac == 3) {
	const char *mdir = av[0];
	const char *mname = av[1];
	const char *ctarg = av[2];

	uint64_t ct;
	int r = strtoull(ctarg, 0, 10, &ct);
	if (r < 0) {
	    printf("bad number: %s\n", ctarg);
	    return;
	}

	struct fs_inode dir, root;
	r = fs_get_root(ct, &root);
	if (r < 0) {
	    printf("fs_get_root(%ld): %s\n", ct, e2s(r));
	    return;
	}

	r = fs_namei(mdir, &dir);
	if (r < 0) {
	    printf("fs_namei(%s): %s\n", mdir, e2s(r));
	    return;
	}

	r = fs_mount(dir, mname, root);
	if (r < 0)
	    printf("fs_mount: %s\n", e2s(r));
    } else {
	printf("Usage: mount\n");
	printf("       mount <directory> <name> <container>\n");
    }
}

static void
builtin_exit(int ac, char **av)
{
    close(0);
}

static struct {
    const char *name;
    const char *desc;
    void (*func) (int ac, char **av);
} commands[] = {
    { "help",	"Display the list of commands",	&builtin_help },
    { "lc",	"List a container",		&builtin_list_container },
    { "spawn",	"Run in background",		&builtin_spawn },
    { "unref",	"Drop container object",	&builtin_unref },
    { "exit",	"Exit",				&builtin_exit },
    { "cd",	"Change directory",		&builtin_cd },
    { "mount",	"Mount a container in the FS",	&builtin_mount },
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

    spawn_and_wait(ac, av);
}

int
main(int ac, char **av)
{
    printf("JOS: shell\n");

    struct fs_inode selfdir;
    int r = fs_get_root(start_env->container, &selfdir);
    if (r < 0) {
	printf("shell: cannot get own container: %s\n", e2s(r));
    } else {
	fs_unmount(start_env->fs_root, "self");
	r = fs_mount(start_env->fs_root, "self", selfdir);
	if (r < 0)
	    printf("shell: cannot mount /self: %s\n", e2s(r));
    }

    for (;;) {
	char prompt[64];
	snprintf(prompt, sizeof(prompt), "[jos:%ld]> ",
		 start_env->fs_cwd.obj.object);
	char *cmd = readline(prompt);
	if (cmd == 0) {
	    printf("EOF\n");
	    break;
	}

	parse_cmd(cmd);
	run_cmd(cmd_argc, cmd_argv);
    }
}
