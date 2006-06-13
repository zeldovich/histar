extern "C" {
#include <inc/types.h> 
#include <inc/lib.h>   
#include <inc/fs.h>

#include <stdio.h>
}

#include <inc/dis/globalcatc.hh>
#include <inc/dis/exportc.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>

export_managerc::export_managerc(void)
{
    int64_t export_ct, manager_gt;
    //error_check(export_ct = container_find(start_env->root_container, kobj_container, "exportd"));
    //error_check(manager_gt = container_find(export_ct, kobj_gate, "manager"));
    gate_ = COBJ(export_ct, manager_gt);
}

export_segmentc 
export_managerc::segment_new(const char *pn)
{
    fs_inode ino;
    error_check(fs_namei(pn, &ino));
    label seg_l, th_l;
    obj_get_label(ino.obj, &seg_l);
    thread_cur_label(&th_l);
    
    global_catc gcat = global_catc();
    
    ulabel *ul = seg_l.to_ulabel();
    
    for (uint64_t i = 0; i < ul->ul_nent; i++) {
        uint64_t h = LB_HANDLE(ul->ul_ent[i]);
        if (label::leq_starhi(h, th_l.get(h)))
            gcat.global(h, 0, true);            
    }
    
    return export_segmentc();    
}
