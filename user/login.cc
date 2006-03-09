extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <stdio.h>
#include <string.h>
}

#include <inc/error.hh>
#include <inc/authclnt.hh>

static void
login(char *u, char *p)
{
    printf("logging in as %s, pw %s\n", u, p);

    uint64_t t, g;
    auth_call(authd_login, u, p, "", &t, &g);

    printf("taint %lu grant %lu\n", t, g);
}

static void
prompt()
{
    char user[32], pass[32];

    char *s = readline("login: ");
    strcpy(user, s);

    char *p = readline("password: ");
    strcpy(pass, p);

    login(&user[0], &pass[0]);
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
