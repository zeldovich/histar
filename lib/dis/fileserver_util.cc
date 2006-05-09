extern "C" {
#include <inc/fs.h>
#include <stdio.h>
#include <fcntl.h>
}

#include <inc/error.hh>
#include <inc/labelutil.hh>

#include <lib/dis/exportsrv.hh>
#include <lib/dis/globallabel.hh>

void
fileserver_acquire(char *path, int mode)
{
    fs_inode ino;
    error_check(fs_namei(path, &ino));
    
    label seg_label;
    obj_get_label(ino.obj, &seg_label);
    label th_label;
    thread_cur_label(&th_label);
    
    //printf("seg_label %s\n", seg_label.to_string());
    //printf("th_label %s\n", th_label.to_string());
    
    int r;
    for (uint64_t i = 0; i < seg_label.to_ulabel()->ul_nent; i++) {
        uint64_t h = LB_HANDLE(seg_label.to_ulabel()->ul_ent[i]);
        r = label_leq_starhi(seg_label.get(h), th_label.get(h));
        if (r < 0)
            export_acquire(h);
    }

    for (uint64_t i = 0; i < th_label.to_ulabel()->ul_nent; i++) {
        uint64_t h = LB_HANDLE(th_label.to_ulabel()->ul_ent[i]);
        r = label_leq_starhi(seg_label.get(h), th_label.get(h));
        if (r < 0)
            export_acquire(h);
    }
    
    if ((O_WRONLY & mode) || (O_RDWR & mode)) { 
        for (uint64_t i = 0; i < th_label.to_ulabel()->ul_nent; i++) {
            uint64_t h = LB_HANDLE(th_label.to_ulabel()->ul_ent[i]);
            r = label_leq_starlo(th_label.get(h), seg_label.get(h));
            if (r < 0)
                export_acquire(h);
        }
    
        for (uint64_t i = 0; i < seg_label.to_ulabel()->ul_nent; i++) {
            uint64_t h = LB_HANDLE(seg_label.to_ulabel()->ul_ent[i]);
            r = label_leq_starlo(th_label.get(h), seg_label.get(h));
            if (r < 0)
                export_acquire(h);
        }    
    }
}

global_label*
fileserver_new_global(char *path)
{
    fs_inode ino;
    error_check(fs_namei(path, &ino));
    label seg_label;
    obj_get_label(ino.obj, &seg_label);
    
    return new global_label(&seg_label);
}
