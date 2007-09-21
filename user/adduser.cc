extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/string.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <inttypes.h>

#include <sys/types.h>
}

#include <inc/gateclnt.hh>
#include <inc/error.hh>
#include <inc/authclnt.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/spawn.hh>

static void __attribute__((noreturn))
usage()
{
    printf("Usage: adduser username [password]\n");
    exit(-1);
}

int
main(int ac, char **av)
{
    if (ac != 2 && ac != 3)
	usage();

    const char *uname = av[1];

    try {
	fs_inode uauth_dir;
	error_check(fs_namei("/uauth", &uauth_dir));

	int64_t user_ct =
	    sys_container_alloc(uauth_dir.obj.object, 0,
				uname, 0, CT_QUOTA_INF);
	error_check(user_ct);

	fs_inode user_authd;
	error_check(fs_namei("/bin/auth_user", &user_authd));

	char root_grant[32];
	sprintf(&root_grant[0], "%"PRIu64, start_env->user_grant);
	const char *argv[] = { "auth_user", root_grant };

	// Avoid giving our user privilege to this new process
	uint64_t ug = start_env->user_grant;
	uint64_t ut = start_env->user_taint;
	start_env->user_grant = start_env->user_taint = 0;

	struct child_process cp =
	    spawn(user_ct, user_authd, 0, 1, 2,
		  2, argv, 0, 0,
		  0, 0, 0, 0, 0);

	start_env->user_grant = ug;
	start_env->user_taint = ut;

	int64_t ec;
	error_check(process_wait(&cp, &ec));

	int64_t uauth_gate;
	error_check(uauth_gate =
	    container_find(cp.container, kobj_gate, "user login gate"));

	int64_t dir_ct, dir_gt;
	error_check(dir_ct = container_find(start_env->root_container, kobj_container, "auth_dir"));
	error_check(dir_gt = container_find(dir_ct, kobj_gate, "authdir"));

	gate_call_data gcd;
	auth_dir_req   *req   = (auth_dir_req *)   &gcd.param_buf[0];
	auth_dir_reply *reply = (auth_dir_reply *) &gcd.param_buf[0];
	req->op = auth_dir_add;
	strcpy(&req->user[0], uname);
	req->user_gate = COBJ(cp.container, uauth_gate);

	label verify;
	thread_cur_label(&verify);
	gate_call(COBJ(dir_ct, dir_gt), 0, 0, 0).call(&gcd, &verify);
	error_check(reply->err);

	uint64_t user_grant, user_taint;
	auth_login(uname, "", &user_grant, &user_taint);

	label l(1);
	l.set(user_grant, 0);
	l.set(user_taint, 3);

	fs_inode home;
	error_check(fs_namei("/home", &home));

	fs_inode uhome;
	error_check(fs_mkdir(home, uname, &uhome, l.to_ulabel()));

	uid_t new_uid = 1000;
	while (getpwuid(new_uid))
	    new_uid++;

	struct passwd pwd;
	pwd.pw_name = (char *) uname;
	pwd.pw_passwd = (char *) "*";
	pwd.pw_uid = new_uid;
	pwd.pw_gid = 0;
	pwd.pw_gecos = (char *) "";
	pwd.pw_shell = (char *) "/bin/ksh";

	char homepath[64];
	snprintf(&homepath[0], sizeof(homepath), "/home/%s", uname);
	pwd.pw_dir = &homepath[0];

	FILE *f = fopen("/etc/passwd", "a");
	putpwent(&pwd, f);
	fclose(f);
    } catch (std::exception &e) {
	printf("adding user: %s\n", e.what());
	return -1;
    }

    if (ac == 3) try {
	const char *pass = av[2];
	auth_chpass(uname, "", pass);
    } catch (std::exception &e) {
	printf("setting password: %s\n", e.what());
	return -1;
    }

    return 0;
}
