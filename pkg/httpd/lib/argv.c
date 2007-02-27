#include <inc/argv.h>

#include <stdio.h>
#include <string.h>

char *
arg_val(struct arg_desc *adv, const char *name)
{
    for (int i = 0; adv[i].name; i++) {
	if (!strcmp(adv[i].name, name)) {
	    return adv[i].val;
	}
    }
    return 0;
}

int
argv_parse(int ac, const char **av, struct arg_desc *adv)
{
    char buf[128], buf2[128];
    for (int i = 1; i < ac; i++) {
	strncpy(buf, av[i], sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = 0;

	if (buf[0] != '-' || buf[1] != '-')
	    continue;

	char *arg = &buf[2];

	char *new_val = strchr(arg, '=');
	if (!new_val) {
	    if (i + 1 < ac) {
		strncpy(buf2, av[i + 1], sizeof(buf2) - 1);
		buf2[sizeof(buf2) - 1] = 0;
		new_val = buf2;
	    } else {
		printf("argv_parse: missing val for: %s\n", arg);
		return -1;
	    }
	} else {
	    *new_val = 0;
	    new_val++;
	}

	char *val = arg_val(adv, arg);
	if (!val) {
	    printf("argv_parse: unknown arg: %s\n", arg);
	    return -1;
	}

	strncpy(val, new_val, sizeof(adv->val) - 1);
	val[sizeof(adv->val) - 1] = 0;
    }
    return 0;
}
