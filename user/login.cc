extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
}

#include <inc/error.hh>
#include <inc/authclnt.hh>
#include <inc/spawn.hh>

static void
login(char *u, char *p)
{
    uint64_t t, g;
    if (auth_call(authd_login, u, p, "", &t, &g) < 0) {
        printf("login failed\n");
        return;    
    }

    printf("logging in as %s, pw %s\n", u, p);
    printf("taint %lu grant %lu\n", t, g);
    
    struct fs_inode fsshell;
    error_check(fs_namei("/bin/ksh", &fsshell));

    label ds(3);
    ds.set(t, LB_LEVEL_STAR);
    ds.set(g, LB_LEVEL_STAR);

    const char *argv[1] = { "/bin/ksh" };
    struct child_process shell = spawn(start_env->shared_container,
                                       fsshell,
                                       0, 1, 2,
                                       1, &argv[0],
                                       0, &ds, 0, 0);
    int64_t e;
    process_wait(&shell, &e);
}

static void
prompt()
{
    char user[32], pass[32];
    
    for (;;) {
        char *s = readline("login: ");
        strcpy(user, s);
    
        char *p = readline("password: ");
        strcpy(pass, p);
        
        login(&user[0], &pass[0]);
    }
}

int
main(int ac, char **av)
{
    try {
	   prompt();
    } catch (std::exception &e) {
	   printf("%s\n", e.what());
    }

    return 0;
}
