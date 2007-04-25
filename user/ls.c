#include <stdio.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>
#include <inc/syscall.h>
#include <string.h>
#include <inttypes.h>

const char *kobj_str[] = {
    "c",
    "t",
    "g",
    "s",
    "a",
    "n",
    "l",
    "d",
    "error ntypes",
    "error any",
};

static struct fs_inode dir = {{0, 0}};
static char long_mode = 0;

static int
on_option(const char *str)
{
    if (!strcmp(str, "l")) {
	long_mode = 1;
	return 0;
    } 
    printf("unknown option: %s\n", str);
    return -E_INVAL;
}


static int
on_dirname(const char *str)
{
    int r = fs_namei(str, &dir);
    if (r < 0) {
	printf("cannot lookup %s: %s\n", str, e2s(r));
	return r;
    }
    return 0;
}

int
main(int ac, char **av)
{
    int r;

    for (int i = 1; i < ac; i++) {
	if (av[i][0] == '-') {
	    if (on_option(&av[i][1]) < 0)
		return -1;
	} else {
	    if (on_dirname(av[i]) < 0)
		return -1;
	}
    }
    
    if (dir.obj.object == 0)
	dir = start_env->fs_cwd;
    
    struct fs_readdir_state s;
    r = fs_readdir_init(&s, dir);
    if (r < 0) {
	printf("fs_readdir_init: %s\n", e2s(r));
	return r;
    }

    for (;;) {
	struct fs_dent de;
	r = fs_readdir_dent(&s, &de, 0);
	if (r < 0) {
	    printf("fs_readdir_dent: %s\n", e2s(r));
	    return r;
	}
	if (r == 0)
	    break;

	if (long_mode) {
	    const char *type = 0;
	    kobject_type_t kobj = sys_obj_get_type(de.de_inode.obj);
	    if ((int32_t)kobj < 0)
		type = "?";
	    else
	    type = kobj_str[kobj];
	    
	    char dev_id = '-';
	    struct fs_object_meta m;
	    r = sys_obj_get_meta(de.de_inode.obj, &m);
	    if ((r >= 0) && (m.dev_id < 123) && (m.dev_id > 47))
		dev_id = m.dev_id;

	    struct ulabel *l = label_alloc();
	    const char *label = 0;
	    r = sys_obj_get_label(de.de_inode.obj, l);
	    for (int i = 0; i < 3 && r < 0; i++) {
		r = label_grow(l);
		if (r < 0) {
		    printf("label_grow: %s\n", e2s(r));
		    return -1;
		}
		r = sys_obj_get_label(de.de_inode.obj, l);
	    }
	    
	    if (r < 0)
		label = "?";
	    else
		label = label_to_string(l);
	    label_free(l);

	    char id[32];
	    snprintf(id, 32, "%"PRIu64, de.de_inode.obj.object);
	    printf("%-20.20s %-1.1s %c %-20.20s %s\n", de.de_name, type, dev_id, id, label);
	} else {
	    printf("%s\n", de.de_name);
	}
    }
}
