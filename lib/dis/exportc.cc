extern "C" {
#include <inc/types.h> 
#include <inc/lib.h>   
#include <inc/fs.h>
}

#include <inc/dis/exportc.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>

export_managerc::export_managerc(void)
{
    int64_t export_ct, manager_gt;
    error_check(export_ct = container_find(start_env->root_container, kobj_container, "exportd"));
    error_check(manager_gt = container_find(export_ct, kobj_gate, "manager"));
    gate_ = COBJ(export_ct, manager_gt);
}

export_segmentc 
export_managerc::segment_new(const char *pn)
{
    fs_inode ino;
    error_check(fs_namei(pn, &ino));
    label seg_label;
    obj_get_label(ino.obj, &seg_label);
    
    // TMP - examine labels and acquire correct handles        
    
    return export_segmentc();    
}
