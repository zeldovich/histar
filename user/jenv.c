#include <stdio.h>
#include <inttypes.h>
#include <inc/lib.h>

int
main(int ac, char **av)
{
#define PRINT_64(f) printf("%20s: %"PRIu64"\n", #f, start_env->f);
#define PRINT_32(f) printf("%20s: %"PRIu32"\n", #f, start_env->f);

    PRINT_64(proc_container);
    PRINT_64(shared_container);
    PRINT_64(root_container);
    PRINT_64(process_pool);

    PRINT_64(user_taint);
    PRINT_64(user_grant);
    PRINT_64(ppid);

    PRINT_32(ruid);
    PRINT_32(euid);
}
