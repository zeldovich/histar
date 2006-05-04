#include <lib/dis/remfiledsrv.hh>

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>

extern "C" {
#include <inc/lib.h>    
#include <stdio.h>
}

int
main (int ac, char **av)
try {
    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    remfiledsrv_create(start_env->shared_container, &th_ctm, &th_clr);
    thread_halt();
} catch (std::exception &e) {
    printf("remfiled: %s\n", e.what());
    return -1;
}
