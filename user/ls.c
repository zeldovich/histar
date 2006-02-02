#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>

int
main(int ac, char **av)
{
    int n = 0;
    for (;;) {
	struct fs_dent de;
	int r = fs_get_dent(start_env->fs_root, n++, &de);
	if (r < 0) {
	    if (r != -E_RANGE)
		printf("fs_get_dent: %s", e2s(r));
	    break;
	}

	printf("%s\n", de.de_name);
    }
}
