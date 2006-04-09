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

enum { login_debug = 0 };

static void
login(char *u, char *p)
{
    authd_reply reply;
    if (auth_call(authd_login, u, p, "", &reply) < 0) {
        printf("login failed\n");
        return;    
    }

    if (login_debug)
	printf("uid %ld taint %lu grant %lu\n", 
	       reply.user_id, reply.user_taint, reply.user_grant);
    
    struct fs_inode fsshell;
    error_check(fs_namei("/bin/ksh", &fsshell));
    
    start_env->user_taint = reply.user_taint;
    start_env->user_grant = reply.user_grant;
    
    const char *argv[1] = { "/bin/ksh" };
    const char *envv[2] = { "TERM=vt100", "TERMINFO=/x/share/terminfo" };
    struct child_process shell = spawn(start_env->shared_container,
                                       fsshell,
                                       0, 1, 2,
                                       1, &argv[0],
                                       2, &envv[0],
                                       0, 0, 0, 0);
    int64_t e;
    process_wait(&shell, &e);
}

static void __attribute__((noreturn))
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
