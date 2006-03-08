extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
}

#include <inc/error.hh>
#include <inc/authclnt.hh>

static void __attribute__((noreturn))
usage()
{
    printf("Usage: uadm (add|delete|chpass) username\n");
    exit(-1);
}

int
main(int ac, char **av)
{
    if (ac != 3)
	usage();

    const char *opstr = av[1];
    const char *uname = av[2];
    const char *pass = "";
    const char *npass = "";

    int op;
    if (!strcmp(opstr, "add"))
	op = authd_adduser;
    else if (!strcmp(opstr, "delete"))
	op = authd_deluser;
    else if (!strcmp(opstr, "chpass"))
	op = authd_chpass;
    else
	usage();

    try {
	if (op == authd_adduser || op == authd_chpass)
	    pass = readline("password: ");
	if (op == authd_chpass)
	    npass = readline("new password: ");

	uint64_t t, g;
	auth_call(op, uname, pass, npass, &t, &g);
    } catch (std::exception &e) {
	printf("%s\n", e.what());
    }

    return 0;
}
