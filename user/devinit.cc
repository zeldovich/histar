extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/gateparam.h>
#include <inc/syscall.h>

#include <stdlib.h>
#include <stdio.h>
}

#include <inc/gatesrv.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>
#include <inc/scopeguard.hh>

void 
spawn_dev(const char *pn, const char *ct, const char *h_grant, label *ds)
{
    struct child_process cp;
    try {
	struct fs_inode ino;
	int r = fs_namei(pn, &ino);
	if (r < 0)
	    throw error(r, "cannot fs_lookup %s", pn);

	const char *argv[] = { pn, ct, h_grant };
	cp = spawn(start_env->shared_container, ino,
	           fileno(stdin), fileno(stdout), fileno(stderr),
	           3, &argv[0],
		   0, 0,
	           0, ds, 0, 0, 0);
    } catch (std::exception &e) {
	cprintf("spawn_dev(%s): %s\n", pn, e.what());
    }
}
	  
int
main (int ac, char **av)
{
    if (ac < 2) {
	cprintf("usage: %s category\n", av[0]);
	exit(-1);
    }
    
    try {
	uint64_t h_grant = handle_alloc();
	scope_guard<void, uint64_t> drop_grant(thread_drop_star, h_grant);

	uint64_t dev_ct;
	label ldev(1);
	ldev.set(h_grant, 0);
	error_check(dev_ct = sys_container_alloc(start_env->root_container, ldev.to_ulabel(), "dev", 0, CT_QUOTA_INF));
	
	// start devs
	char shared_buf[32];
	snprintf(&shared_buf[0], sizeof(shared_buf), "%lu", dev_ct);

	label ds_hgrant(3);
	ds_hgrant.set(h_grant, LB_LEVEL_STAR);
	char grant_buf[32];
	snprintf(&grant_buf[0], sizeof(grant_buf), "%lu", h_grant);

	spawn_dev("/bin/devpt", shared_buf, grant_buf, &ds_hgrant);
    } catch (basic_exception e) {
	cprintf("dev fatal: %s\n", e.what());
	exit(-1);
    }
    
    thread_halt();
}
