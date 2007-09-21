extern "C" {
#include <inc/syscall.h>
#include <string.h>
#include <inc/error.h>
}

#include <inc/fs_dir.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>

int
fs_dir_ct::list(fs_readdir_pos *i, fs_dent *de)
{
 retry:
    int64_t id = sys_container_get_slot_id(ino_.obj.object, i->a++);
    if (id < 0) {
	if (id == -E_INVAL)
	    return 0;
	if (id == -E_NOT_FOUND)
	    goto retry;
	throw error(id, "sys_container_get_slot_id");
    }

    de->de_inode.obj = COBJ(ino_.obj.object, id);
    error_check(sys_obj_get_name(de->de_inode.obj, &de->de_name[0]));
    return 1;
}

void
fs_dir_ct::insert(const char *name, fs_inode ino)
{
    if (ino.obj.container != ino_.obj.object)
	throw basic_exception("fs_dir_ct::insert: wrong container");
}

void
fs_dir_ct::remove(const char *name, fs_inode ino)
{
    if (ino.obj.container != ino_.obj.object)
	throw basic_exception("fs_dir_ct::remove: wrong container");
}

int
fs_dir::lookup(const char *name, fs_readdir_pos *i, fs_inode *ino)
{
    for (;;) {
	struct fs_dent de;
	int r = list(i, &de);
	if (r == 0)
	    return 0;
	if (!strcmp(name, de.de_name)) {
	    *ino = de.de_inode;
	    return 1;
	}
    }
}
