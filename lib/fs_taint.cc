extern "C" {
#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/stdio.h>
}

#include <inc/cpplabel.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/selftaint.hh>

int
fs_taint_self(struct fs_inode f)
try
{
    label fl;
    obj_get_label(f.obj, &fl);
    taint_self(&fl);
    return 0;
} catch (error &e) {
    cprintf("fs_taint_self: %s\n", e.what());
    return e.err();
}
