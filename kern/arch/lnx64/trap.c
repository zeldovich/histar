#include <kern/arch.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
thread_arch_run(const struct Thread *t)
{
    printf("thread_arch_run(%"PRIu64"): bummer\n", t->th_ko.ko_id);
    exit(-1);
}
