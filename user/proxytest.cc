extern "C" {
#include <inc/lib.h>    
#include <inc/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int remfile_open(char *host, char *path);

}

#include <inc/labelutil.hh>

#include <lib/dis/proxydclnt.hh>
#include <lib/dis/exportclnt.hh>



int
main (int ac, char **av)
{
#if 0
    uint64_t handle0 = handle_alloc();
    uint64_t handle1 = handle_alloc();

    proxyd_add_mapping((char*)"hello handle", handle0, handle1, 0);
 
    thread_drop_star(handle0);
    
    label th_l;
    label th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);
    
    int64_t h =  proxyd_get_local((char*)"hello handle");
    if (h < 0) {
        printf("h (%ld) < 0\n", h);
        exit(-1);
    }
   
    if ((uint64_t)h != handle0) {
        printf("h (%ld) != handle (%ld)\n", h, handle0);
        exit(-1);
    }

    char buf[16];

    if (proxyd_get_global(handle0, buf) < 0) {
        printf("h (%ld) != handle (%ld)\n", h, handle0);
        exit(-1);
    }
    printf("global %s\n", buf);
#endif

    struct fs_inode tmp;
    if (fs_namei("/tmp", &tmp) < 0) {
        printf("couldn't open tmp\n");
        exit(-1);
    }

    uint64_t taint0 = handle_alloc();
    uint64_t grant0_fs = handle_alloc();
    proxyd_add_mapping((char*)"test0 handle", taint0, grant0_fs, 0);
    export_grant(grant0_fs, taint0);

    struct fs_inode test0;
    label l0(1);
    l0.set(taint0, 3);
    if (fs_create(tmp, "test0", &test0, l0.to_ulabel()) < 0) {
        printf("couldn't create test0\n");
        exit(-1);    
    }
    
    fs_pwrite(test0, "hello world", sizeof("hello world"), 0);
    return 0;    
}

