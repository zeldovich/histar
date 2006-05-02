extern "C" {
#include <inc/lib.h>    
#include <inc/syscall.h>
#include <stdio.h>
#include <stdlib.h>
}

#include <inc/labelutil.hh>

#include <lib/dis/proxydclnt.hh>

int
main (int ac, char **av)
{
    uint64_t handle0 = handle_alloc();
    uint64_t handle1 = handle_alloc();

    proxyd_addmapping((char*)"hello handle", handle0, handle1, 0);
 
    thread_drop_star(handle0);
    
    label th_l;
    label th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);
    
    int64_t h =  proxyd_gethandle((char*)"hello handle");
    if (h < 0) {
        printf("h (%ld) < 0\n", h);
        exit(-1);
    }
   
    if ((uint64_t)h != handle0) {
        printf("h (%ld) != handle (%ld)\n", h, handle0);
        exit(-1);
    }
    
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);
    
    return 0;    
}
