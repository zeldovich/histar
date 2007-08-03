extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
}

#include <inc/error.hh>
#include <inc/authclnt.hh>

static void __attribute__((noreturn))
usage()
{
    printf("Usage: passwd username\n");
    exit(-1);
}

int
main(int ac, char **av)
{
    if (ac != 2)
	usage();

    const char *uname = av[1];
    const char *x;

    char *pass, *npass;
    x = readline("password: ", 0);
    printf("\n");
    if (!x)
	return -1;
    pass = strdup(x);

    x = readline("new password: ", 0);
    printf("\n");
    if (!x)
	return -1;
    npass = strdup(x);

    try {
	auth_chpass(uname, pass, npass);
    } catch (std::exception &e) {
	printf("%s\n", e.what());
    }

    return 0;
}
