extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/error.h>

#include <string.h>
#include <stdio.h>
}

#include <inc/dis/catd.hh>

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>


#define NUM_MAPPINGS 16
struct {
    uint64_t local;
    cobj_ref grant_gt;    
} mapping[NUM_MAPPINGS];

static void __attribute__((noreturn))
grantcat(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    uint64_t local = (uint64_t) arg;
    label *dl = new label(3);
    dl->set(local, LB_LEVEL_STAR);
    gr->ret(0, dl, 0);        
}

static void
add_mapping(cd_arg *arg)
{
    char buffer[32];
    for (int i = 0; i < NUM_MAPPINGS; i++) {
        if (!mapping[i].local) {
            mapping[i].local = arg->add.local;
            // make a gate
            label th_l, th_cl;
            thread_cur_label(&th_l);
            thread_cur_clearance(&th_cl);
            th_l.set(arg->add.local, LB_LEVEL_STAR);
            th_cl.set(start_env->process_grant, 0);
            
            sprintf(buffer, "%ld", arg->add.local);

            cobj_ref gt = gate_create(start_env->shared_container, buffer, &th_l, 
                             &th_cl, &grantcat, (void *)arg->add.local);
            mapping[i].grant_gt = gt;
            arg->status = 0;
            return;                
        }
    }
}

static void
rem_mapping(cd_arg *args)
{
    throw basic_exception("rem_mapping: not implemented");
}

static void
acquire(uint64_t local)
{
    for (int i = 0; i < NUM_MAPPINGS; i++) {
        if (mapping[i].local == local) {
            gate_call_data gcd;
            gate_call(mapping[i].grant_gt, 0, 0, 0).call(&gcd, 0);
            return;                
        }
    }
    throw basic_exception("unable to aqcuire %ld", local);    
}

static void
package(cd_arg *arg)
{
    fs_inode file;
    error_check(fs_namei(arg->package.path, &file));
    uint64_t ct = arg->package.cipher_ct;

    label fl;
    obj_get_label(file.obj, &fl);
    ulabel *ufl = fl.to_ulabel();
    for (uint32_t i = 0; i < ufl->ul_nent; i++)
        acquire(LB_HANDLE(ufl->ul_ent[i]));   
    
    label l(1);
    int64_t id;
    error_check(id = sys_segment_copy(file.obj, ct,
                              l.to_ulabel(), "cipher seg"));
    arg->status = 0;
    arg->package.seg = COBJ(ct, id);
    return;    
}

static void
write(cd_arg *arg)
{
    cobj_ref seg = arg->write.seg;
    int count = arg->write.len;
    int offset = arg->write.off;
    void *va = 0;
    error_check(segment_map(seg, 0, SEGMAP_READ, &va, 0, 0));
    scope_guard<int, void *> unmap_va(segment_unmap, va);
    
    fs_inode file;
    error_check(fs_namei(arg->write.path, &file));

    label fl;
    obj_get_label(file.obj, &fl);
    ulabel *ufl = fl.to_ulabel();
    for (uint32_t i = 0; i < ufl->ul_nent; i++)
        acquire(LB_HANDLE(ufl->ul_ent[i]));   
    
    arg->status = fs_pwrite(file, va, count, offset);
    return;    
}


static void __attribute__((noreturn))
catd(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    memcpy(&bck, parm, sizeof(bck));
    cd_arg *args = (cd_arg *) parm->param_buf;
    try {
        switch (args->op) {
            case cd_add:
                add_mapping(args);
                break;    
            case cd_rem:
                rem_mapping(args);
                break;
            case cd_package:
                package(args);
                break;
            case cd_write:
                write(args);
                break;
        }
    } catch (basic_exception e) {
        printf("catd: %s\n", e.what());
        args->status = -1;    
    }
    memcpy(parm, &bck, sizeof(*parm) - sizeof(parm->param_buf));
    gr->ret(0, 0, 0);    
}


int
main (int ac, char **av)
{
    label th_l, th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);

    gate_create(start_env->shared_container,"catd gate", &th_l, 
                &th_cl, &catd, 0);
    memset(mapping, 0, sizeof(mapping));

    printf("catd: inited with K %ld\n", 0L);

    thread_halt();
    return 0;    
}
