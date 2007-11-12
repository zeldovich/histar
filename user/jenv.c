#include <stdio.h>
#include <inttypes.h>
#include <inc/lib.h>

int
main(int ac, char **av)
{
#define PRINT_64(f)  printf("%20s: %"PRIu64"\n", #f, start_env->f);
#define PRINT_32(f)  printf("%20s: %"PRIu32"\n", #f, start_env->f);
#define PRINT_OBJ(f) printf("%20s: %"PRIu64".%"PRIu64"\n", #f, \
			    start_env->f.container, start_env->f.object);

    PRINT_64(proc_container);
    PRINT_64(shared_container);
    PRINT_64(root_container);
    PRINT_64(process_pool);

    PRINT_64(user_taint);
    PRINT_64(user_grant);
    PRINT_64(ppid);

    PRINT_OBJ(process_gid_seg);
    PRINT_OBJ(declassify_gate);
    PRINT_OBJ(time_seg);
    PRINT_OBJ(fs_mtab_seg);

    PRINT_32(ruid);
    PRINT_32(euid);
}
