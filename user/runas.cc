extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <inttypes.h>
#include <login_cap.h>
}

#include <inc/error.hh>
#include <inc/authclnt.hh>
#include <inc/spawn.hh>

static void
login(char *u, char *p)
{
    struct passwd *pw = getpwnam(u);
    if (!pw) {
	printf("unknown user: %s\n", u);
	exit(-1);
    }

    uint64_t ug, ut;
    try {
	auth_login(u, p, &ug, &ut);
    } catch (std::exception &e) {
	printf("login failed: %s\n", e.what());
	exit(-1);
    }

    start_env->user_taint = ut;
    start_env->user_grant = ug;

    setenv("USER", u, 1);
    setenv("HOME", pw->pw_dir, 1);
    chdir(pw->pw_dir);

    setusercontext(0, pw, pw->pw_uid, LOGIN_SETALL);
}

int
main(int ac, char **av)
{
    if (ac < 3) {
	printf("Usage: %s username password command\n", av[0]);
	exit(-1);
    }

    login(av[1], av[2]);
    execv(av[3], av + 3);
    perror("execv");
    return -1;
}
