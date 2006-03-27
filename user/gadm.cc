extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
}

#include <inc/error.hh>
#include <inc/groupclnt.hh>
#include <inc/authclnt.hh>

static void __attribute__((noreturn))
usage()
{
    printf("Usage: gadm (add|wadd|radd|udel) groupname username\n");
    exit(-1);
}

int
main(int ac, char **av)
{
    if (ac != 4)
	   usage();

    const char *opstr = av[1];
    const char *gname = av[2];
    const char *uname = av[3];
    int type = 0;
    uint64_t t, g;
    auth_unamehandles(uname, &t, &g); 
    int op;

    if (!strcmp(opstr, "add")) {
	   op = authd_addgroup;
    } else if (!strcmp(opstr, "wadd")) {
        op = authd_addtogroup;
        type = group_write;
    } else if (!strcmp(opstr, "radd")) {
        op = authd_addtogroup;
        type = group_read;
    } else if (!strcmp(opstr, "udel")) {
        op = authd_delfromgroup;
    } else
	   usage();
    try {
    	auth_groupcall(op, type, gname, t, g, 0);
    } catch (std::exception &e) {
	   printf("%s\n", e.what());
    }

    return 0;
}
