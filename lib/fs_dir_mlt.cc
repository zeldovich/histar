extern "C" {
#include <inc/syscall.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <inc/mlt.h>
}

#include <inc/fs_dir.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>

int
fs_dir_mlt::list(fs_readdir_pos *i, fs_dent *de)
{
    return 0;

#if 0
    uint8_t buf[MLT_BUF_SIZE];
    uint64_t ct;
    int r = sys_mlt_get(ino_.obj, i->a++, 0, &buf[0], &ct);
    if (r < 0) {
	if (r == -E_NOT_FOUND)
	    return 0;
	throw error(r, "fs_dir_mlt::list: sys_mlt_get");
    }

    de->de_inode.obj = COBJ(ct, ct);
    snprintf(&de->de_name[0], sizeof(de->de_name), "%lu", ct);
    return 1;
#endif
}
