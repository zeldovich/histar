extern "C" {
#include <inc/types.h>
#include <inc/lib.h>    
#include <stdio.h>
}
#include <lib/dis/proxydsrv.hh>

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>


int
main (int ac, char **av)
try {
    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    proxydsrv_create(start_env->shared_container, "proxyd", &th_ctm, &th_clr);
    thread_halt();
} catch (std::exception &e) {
    printf("proxyd: %s\n", e.what());
    return -1;
}
