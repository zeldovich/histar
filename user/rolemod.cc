extern "C" {
#include <inc/stdio.h>
#include <inc/fs.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
}

#include <inc/spawn.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/rbac.hh>

static void __attribute__((__noreturn__))
usage(const char *msg)
{
    cprintf("error: %s\n", msg);
    cprintf("usage: rolemod op [op-args]\n");
    exit(-1);
}

static void
on_assoc(const char *role_pn, const char *trans_pn)
{
    struct cobj_ref role_gate = rbac::fs_object(role_pn);
    struct cobj_ref trans_gate = rbac::fs_object(trans_pn);
    rbac::gate_send(role_gate, trans_gate);
}

static void
spawn_x(const char *bin_pn, const char *arg_dir_pn, 
	const char *arg_name, uint64_t grant_root, label *ds)
{
    struct fs_inode ino;
    int r = fs_namei(bin_pn, &ino);
    if (r < 0)
	throw error(r, "cannot fs_namei %s", bin_pn);
    
    char buf_grant[32];
    snprintf(buf_grant, sizeof(buf_grant), "%ld", grant_root);

    struct fs_inode ct_ino;
    r = fs_namei(arg_dir_pn, &ct_ino);
    if (r < 0)
	throw error(r, "cannot fs_namei %s", arg_dir_pn);
    char buf_ct[32];
    snprintf(buf_ct, sizeof(buf_ct), "%ld", ct_ino.obj.object);
    
    const char *argv[] = { bin_pn, arg_name, buf_grant, buf_ct };
    
    struct child_process cp;
    cp = spawn(start_env->shared_container, ino,
	       0, 0, 0,
	       4, &argv[0],
	       0, 0,
	       0, ds, 0, 0, 0);
}

static void
on_role(const char *role_name, uint64_t grant_root)
{
    const char pn[] = "/bin/rbac_role";
    const char roles_pn[] = "/roles";

    label ds(3);
    ds.set(grant_root, LB_LEVEL_STAR);
    spawn_x(pn, roles_pn, role_name, grant_root, &ds);
}

static void
on_trans(int ac, char **av)
{
    label tl;
    thread_cur_label(&tl);
    if (tl.get(1) != LB_LEVEL_STAR)
	usage("must be invoked with root grant-category");
    uint64_t grant_root = 1;
    
    if (ac < 3)
	usage("no bin pathname given");
    if (ac < 4)
	usage("no trans name given");
    
    label ds(3);
    for (int i = 4; i < ac; i++)
	ds.set(atol(av[i]), LB_LEVEL_STAR);
    ds.set(grant_root, LB_LEVEL_STAR);

    char *pn = av[2];
    char *name = av[3];
    const char trans_pn[] = "/trans";
    spawn_x(pn, trans_pn, name, grant_root, &ds);
    
    return;
}

int
main (int ac, char **av)
{
    if (ac < 2)
	usage("no op given");

    label tl;
    thread_cur_label(&tl);
    if (tl.get(1) != LB_LEVEL_STAR)
	usage("must be invoked with root grant-category");
    uint64_t grant_root = 1;
    
    char *op = av[1];
    if (!strcmp(op, "assoc")) {
	if (ac < 3) 
	    usage("missing role path");
	if (ac < 4)
	    usage("missing trans path");
	char *role_pn = av[2];
	char *trans_pn = av[3];
	on_assoc(role_pn, trans_pn);
    } else if (!strcmp(op, "role")) {
	if (ac < 3)
	    usage("missing role name");
	char *role_name = av[2];
	on_role(role_name, grant_root);
    } else if (!strcmp(op, "trans")) {
	on_trans(ac, av);
    } else
	usage("op not reconized");
    
    return 0;
}
