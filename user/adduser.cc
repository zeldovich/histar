extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>

#include <sys/types.h>
}

#include <inc/error.hh>
#include <inc/authclnt.hh>
#include <inc/cpplabel.hh>

static void __attribute__((noreturn))
usage()
{
    printf("Usage: adduser username\n");
    exit(-1);
}

int
main(int ac, char **av)
{
    if (ac != 2)
	usage();

    const char *uname = av[1];
    const char *pass = readline("password: ");
    if (!pass)
	usage();

    try {
	authd_reply r;
	auth_call(authd_adduser, uname, pass, "", &r);

	label l(1);
	l.set(r.user_grant, 0);
	l.set(r.user_taint, 3);

	fs_inode home;
	error_check(fs_namei("/home", &home));

	fs_inode uhome;
	error_check(fs_mkdir(home, uname, &uhome, l.to_ulabel()));

	struct passwd pwd;
	pwd.pw_name = (char *) uname;
	pwd.pw_passwd = (char *) "*";
	pwd.pw_uid = r.user_id;
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
	printf("%s\n", e.what());
    }

    return 0;
}
