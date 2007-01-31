extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/labelutil.h>
#include <inc/error.h>

#include <stdio.h>
}

#include <inc/error.hh>
#include <inc/spawn.hh>
#include <inc/fs_dir.hh>
#include <inc/scopeguard.hh>

static char eproc_enable = 1;

static struct fs_inode
fs_inode_for(const char *pn)
{
    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r < 0)
	throw error(r, "cannot fs_lookup %s", pn);
    return ino;
}

static void
segment_copy_to_file(struct cobj_ref seg, struct fs_inode dir, 
		     const char *fn, label *l)
{
    struct fs_inode ino;
    error_check(fs_create(dir, fn, &ino, l->to_ulabel()));

    void *va = 0;
    uint64_t bytes = 0;
    error_check(segment_map(seg, 0, SEGMAP_READ, &va, &bytes, 0));
    scope_guard<int, void *> unmap(segment_unmap, va);
    error_check(fs_pwrite(ino, va, bytes, 0));
}

int
main (int ac, char **av)
{
    static const char *default_server_pem = "/bin/server.pem";
    static const char *default_servkey_pem = "/bin/servkey.pem";
    static const char *default_dh_pem = "/bin/dh.pem";

    static const char *ssld_server_pem = "/httpd/ssld-priv/server.pem";
    static const char *ssld_dh_pem = "/httpd/ssld-priv/dh.pem";

    static const char *ssld_servkey_pem = "/httpd/servkey-priv/servkey.pem";

    int64_t ssld_taint = handle_alloc();
    int64_t eprocd_taint = eproc_enable ? handle_alloc() : ssld_taint;
    int64_t access_grant = handle_alloc();
    error_check(ssld_taint);
    error_check(eprocd_taint);
    error_check(access_grant);
    label ssld_label(1);
    ssld_label.set(ssld_taint, 3);
    ssld_label.set(access_grant, 0);
    label eprocd_label(1);
    eprocd_label.set(eprocd_taint, 3);
    eprocd_label.set(access_grant, 0);
    
    int64_t httpd_ct = sys_container_alloc(start_env->root_container, 0,
					   "httpd", 0, CT_QUOTA_INF);
    error_check(httpd_ct);

    struct fs_inode httpd_dir_ino;
    httpd_dir_ino.obj = COBJ(start_env->root_container, httpd_ct);

    struct fs_inode ssld_dir_ino;
    error_check(fs_mkdir(httpd_dir_ino, "ssld-priv", &ssld_dir_ino, 
			 ssld_label.to_ulabel()));

    struct fs_inode eprocd_dir_ino;
    error_check(fs_mkdir(httpd_dir_ino, "servkey-priv", &eprocd_dir_ino, 
			 eprocd_label.to_ulabel()));
        
    struct fs_inode server_pem_ino = fs_inode_for(default_server_pem);
    struct fs_inode dh_pem_ino = fs_inode_for(default_dh_pem);

    struct fs_inode servkey_pem_ino = fs_inode_for(default_servkey_pem);

    segment_copy_to_file(server_pem_ino.obj, ssld_dir_ino, "server.pem", 
			 &ssld_label);
    segment_copy_to_file(dh_pem_ino.obj, ssld_dir_ino, "dh.pem", 
			 &ssld_label);

    segment_copy_to_file(servkey_pem_ino.obj, eprocd_dir_ino, "servkey.pem", 
			 &eprocd_label);

    label ssld_ds(3), ssld_dr(0);
    ssld_ds.set(ssld_taint, LB_LEVEL_STAR);
    ssld_dr.set(ssld_taint, 3);
    
    const char *ssld_pn = "/bin/ssld";
    struct fs_inode ssld_ino = fs_inode_for(ssld_pn);
    const char *ssld_argv[] = { ssld_pn, ssld_server_pem,
				ssld_dh_pem, ssld_servkey_pem };
    struct child_process cp = spawn(httpd_ct, ssld_ino,
				    0, 0, 0,
				    eproc_enable ? 3 : 4, &ssld_argv[0],
				    0, 0,
				    0, &ssld_ds, 0, &ssld_dr, 0,
				    SPAWN_NO_AUTOGRANT);
    int64_t exit_code = 0;
    process_wait(&cp, &exit_code);
    if (exit_code)
	throw error(exit_code, "error starting ssld");
    
    if (eproc_enable) {
	label eprocd_ds(3), eprocd_dr(0);
	eprocd_ds.set(eprocd_taint, LB_LEVEL_STAR);
	eprocd_dr.set(eprocd_taint, 3);
	
	const char *eprocd_pn = "/bin/ssl_eprocd";
	struct fs_inode eprocd_ino = fs_inode_for(eprocd_pn);
	const char *eprocd_argv[] = { eprocd_pn, ssld_servkey_pem };
	cp = spawn(httpd_ct, eprocd_ino,
		   0, 0, 0,
		   2, &eprocd_argv[0],
		   0, 0,
		   0, &eprocd_ds, 0, &eprocd_dr, 0,
		   SPAWN_NO_AUTOGRANT);
	exit_code = 0;
	process_wait(&cp, &exit_code);
	if (exit_code)
	    throw error(exit_code, "error starting ssl_eprocd");
    }
	
    label httpd_ds(3);
    httpd_ds.set(access_grant, LB_LEVEL_STAR);
    label httpd_dr(0);
    httpd_dr.set(access_grant, 3);

    const char *httpd_pn = "/bin/httpd";
    struct fs_inode httpd_ino = fs_inode_for(httpd_pn);
    const char *httpd_argv[] = { httpd_pn };
    spawn(httpd_ct, httpd_ino,
	  0, 0, 0,
	  1, &httpd_argv[0],
	  0, 0,
	  0, &httpd_ds, 0, &httpd_dr, 0, SPAWN_NO_AUTOGRANT);

    return 0;
}
