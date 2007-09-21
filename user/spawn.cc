extern "C" {
#include <inc/fs.h>
#include <inc/syscall.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}

#include <inc/error.hh>
#include <inc/spawn.hh>
#include <inc/cpplabel.hh>


static uint64_t
create_containers(const char *pn)
{
    char *copy = strdup(pn);
    const char *parent;
    
    char *dir[64];
    int dir_count = 0;
    struct fs_inode ino;
    
    while (1) {
	const char *temp;
	fs_dirbase(copy, &parent, &temp);
	if (strcmp(temp, ""))
	    dir[dir_count++] = strdup(temp);

	int r = fs_namei(parent, &ino);
	if (r == 0)
	    break;	    
    }
    
    int64_t ct = ino.obj.object;
    label l(1);
    l.set(start_env->user_grant, 0);
    for (int i = dir_count - 1; i >= 0; i--) {
	ct = sys_container_alloc(ct, l.to_ulabel(), dir[i], 0, CT_QUOTA_INF);
	if (ct < 0)
	    break;
    }
    
    for (int i = 0; i < dir_count; i++)
	free(dir[i]);
    free(copy);
    
    return ct;
}

int 
main (int ac, char **av)
{
    if (ac < 3) {
	printf("usage: %s parent-path filename [args]\n", av[0]);
	exit(-1);
    }
    
    struct fs_inode ct_ino;
    uint64_t ct = 0;
    int r = fs_namei(av[1], &ct_ino);
    if (r < 0)
	ct = create_containers(av[1]);
    else 
	ct = ct_ino.obj.object;

    struct fs_inode ino;
    r = fs_namei(av[2], &ino);
    if (r < 0)
	throw error(r, "cannot fs_namei%s", av[2]);
    
    const char *argv[ac - 2];
    argv[0] = av[2];
    for (int i = 1; i < ac - 2; i++)
	argv[i] = av[i + 2];
    
    struct child_process cp = spawn(ct, ino,
				    0, 0, 0,
				    ac - 2, &argv[0],
				    0, 0,
				    0, 0, 0, 0, 0);
    char buf[64];
    error_check(sys_obj_get_name(COBJ(cp.container, cp.container), buf));
    if (av[1][strlen(av[1]) - 1] == '/')
	printf("%s -> %s%s (%lu)\n", av[2], av[1], buf, cp.container);
    else 
	printf("%s -> %s/%s (%lu)\n", av[2], av[1], buf, cp.container);
}

