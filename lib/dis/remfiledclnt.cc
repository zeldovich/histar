extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/remfile.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <string.h>
#include <stdio.h>
}

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/error.hh>

#include <lib/dis/remfiledsrv.hh>
#include <lib/dis/fileclient.hh>

static cobj_ref
remfiled_server(void)
{
    int64_t dir_ct, dir_gt;
    error_check(dir_ct = container_find(start_env->root_container, kobj_container, "remfiled"));
    error_check(dir_gt = container_find(dir_ct, kobj_gate, "remfiled server"));
    
    return COBJ(dir_ct, dir_gt);
}

ssize_t
remfiled_read(struct rem_inode f, void *buf, uint64_t count, uint64_t off)
{
    gate_call_data gcd;
    remfiled_args *args = (remfiled_args *) gcd.param_buf;

    cobj_ref server_gate = remfiled_server();
    args->op = rf_read;
    args->ino = f;
    args->count = count;
    args->off = off;
    args->grant = start_env->process_grant;
    args->taint = handle_alloc();
    
    label dl(1);
    dl.set(args->grant, LB_LEVEL_STAR);
    dl.set(args->taint, LB_LEVEL_STAR);
    
    gate_call(server_gate, 0, &dl, 0).call(&gcd, 0);
    
    if (args->count) {
        void *va = 0;
        error_check(segment_map(args->seg, SEGMAP_READ, &va, 0));
        memcpy(buf, va, args->count);
        segment_unmap(va);
        sys_obj_unref(args->seg);
    }
    thread_drop_star(args->taint);
    return args->count;    
}

ssize_t 
remfiled_write(struct rem_inode f, const void *buf, uint64_t count, uint64_t off) 
{
    gate_call_data gcd;
    remfiled_args *args = (remfiled_args *) gcd.param_buf;

    cobj_ref server_gate = remfiled_server();
    args->op = rf_write;
    args->ino = f;
    args->count = count;
    args->off = off;
    args->taint = handle_alloc();
 
    struct cobj_ref seg;
    label l(1);
    l.set(start_env->process_grant, 0);
    l.set(args->taint, 3);
    void *va = 0;
    error_check(segment_alloc(start_env->shared_container, count, &seg, &va,
                l.to_ulabel(), "remfiled write buf"));
    memcpy(va, buf, count);
    segment_unmap(va);
    args->seg = seg;

    label dl(1);
    dl.set(args->taint, LB_LEVEL_STAR);
    gate_call(server_gate, 0, &dl, 0).call(&gcd, 0);
    
    sys_obj_unref(seg);
    thread_drop_star(args->taint);
    return args->count;    
}

int 
remfiled_open(char *host, int port, char *path, struct rem_inode *ino)
{
    gate_call_data gcd;
    remfiled_args *args = (remfiled_args *) gcd.param_buf;
    
    cobj_ref server_gate = remfiled_server();
    args->op = rf_open;
    if (strlen(host) + 1 > sizeof(args->host)) {
        printf("remfiled_open: host exceeds %ld\n", sizeof(args->host));
        return -E_NO_SPACE;    
    }
    strcpy(args->host, host);
    if (strlen(path) + 1 > sizeof(args->path)) {
        printf("remfiled_open: path exceeds %ld\n", sizeof(args->path));    
        return -E_NO_SPACE;    
    }
    strcpy(args->path, path);
   
    gate_call(server_gate, 0, 0, 0).call(&gcd, 0);
    memcpy(ino, &args->ino, sizeof(args->ino));
    return 0;    
}
