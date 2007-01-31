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
    static const char *ssld_servkey_pem = "/httpd/ssld-priv/servkey.pem";
    static const char *ssld_dh_pem = "/httpd/ssld-priv/dh.pem";

    int64_t secret_taint = handle_alloc();
    int64_t access_grant = handle_alloc();
    error_check(secret_taint);
    error_check(access_grant);
    label secret_label(1);
    secret_label.set(secret_taint, 3);

    int64_t httpd_ct = sys_container_alloc(start_env->root_container, 0,
					   "httpd", 0, CT_QUOTA_INF);
    error_check(httpd_ct);

    struct fs_inode httpd_dir_ino;
    httpd_dir_ino.obj = COBJ(start_env->root_container, httpd_ct);

    struct fs_inode priv_dir_ino;
    error_check(fs_mkdir(httpd_dir_ino, "ssld-priv", &priv_dir_ino, 
			 secret_label.to_ulabel()));
        
    struct fs_inode server_pem_ino = fs_inode_for(default_server_pem);
    struct fs_inode servkey_pem_ino = fs_inode_for(default_servkey_pem);
    struct fs_inode dh_pem_ino = fs_inode_for(default_dh_pem);


    segment_copy_to_file(server_pem_ino.obj, priv_dir_ino, "server.pem", 
			 &secret_label);
    segment_copy_to_file(servkey_pem_ino.obj, priv_dir_ino, "servkey.pem", 
			 &secret_label);
    segment_copy_to_file(dh_pem_ino.obj, priv_dir_ino, "dh.pem", 
			 &secret_label);

    static char ssld_access_grant[32];
    snprintf(ssld_access_grant, sizeof(ssld_access_grant), "%lu", access_grant);

    label ssld_ds(3), ssld_dr(0);
    ssld_ds.set(access_grant, LB_LEVEL_STAR);
    ssld_ds.set(secret_taint, LB_LEVEL_STAR);
    ssld_dr.set(access_grant, 3);
    ssld_dr.set(secret_taint, 3);
    
    const char *ssld_pn = "/bin/ssld";
    struct fs_inode ssld_ino = fs_inode_for(ssld_pn);
    const char *ssld_argv[] = { ssld_pn, ssld_server_pem,
				ssld_dh_pem, 
				ssld_access_grant};
    struct child_process cp = spawn(httpd_ct, ssld_ino,
				    0, 0, 0,
				    4, &ssld_argv[0],
				    0, 0,
				    0, &ssld_ds, 0, &ssld_dr, 0,
				    SPAWN_NO_AUTOGRANT);
    int64_t exit_code = 0;
    process_wait(&cp, &exit_code);
    if (exit_code)
	throw error(exit_code, "error starting ssld");

    const char *eprocd_pn = "/bin/ssl_eprocd";
    struct fs_inode eprocd_ino = fs_inode_for(eprocd_pn);
    const char *eprocd_argv[] = { eprocd_pn, ssld_servkey_pem};
    cp = spawn(httpd_ct, eprocd_ino,
	       0, 0, 0,
	       2, &eprocd_argv[0],
	       0, 0,
	       0, &ssld_ds, 0, &ssld_dr, 0,
	       SPAWN_NO_AUTOGRANT);
    exit_code = 0;
    process_wait(&cp, &exit_code);
    if (exit_code)
	throw error(exit_code, "error starting ssl_eprocd");


    
    label httpd_ds(3);
    httpd_ds.set(access_grant, LB_LEVEL_STAR);
    label httpd_dr(0);
    httpd_dr.set(access_grant, 3);

    const char *httpd_pn = "/bin/httpd";
    struct fs_inode httpd_ino = fs_inode_for(httpd_pn);
    const char *httpd_argv[] = { httpd_pn, ssld_access_grant };
    spawn(httpd_ct, httpd_ino,
	  0, 0, 0,
	  2, &httpd_argv[0],
	  0, 0,
	  0, &httpd_ds, 0, &httpd_dr, 0, SPAWN_NO_AUTOGRANT);

    return 0;
}
