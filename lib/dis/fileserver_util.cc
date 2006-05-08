extern "C" {
#include <inc/fs.h>
#include <stdio.h>
}

#include <inc/error.hh>
#include <inc/labelutil.hh>

void
fileserver_acquire(char *path, int mode)
{
    fs_inode ino;
    error_check(fs_namei(path, &ino));
    
    label seg_label;
    obj_get_label(ino.obj, &seg_label);
    label th_label;
    thread_cur_label(&th_label);
    
    int r = seg_label.compare(&th_label, label_leq_starhi);
    if (r < 0)
        throw basic_exception("unable to aquire handles for %s", path);
}
