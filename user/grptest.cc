extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inc/stdio.h>
}

#include <inc/groupclnt.hh>
#include <inc/labelutil.hh>
#include <inc/authclnt.hh>
#include <inc/error.hh>
#include <inc/spawn.hh>

static char file_string[32] = "some text.  test";

static void __attribute__((noreturn))
usage()
{
    printf("Usage: grptest group user create|read|write\n");
    exit(-1);
}

static void
create(uint64_t taint, uint64_t grant)
{
    int r;
    label l(1);
    l.set(grant, 0);
    l.set(taint, 3);

    char dst_pn[32] = "/x/grptest.txt";
    const char *dir, *fn;
    fs_dirbase(dst_pn, &dir, &fn);

    struct fs_inode dst_dir;
    r = fs_namei(dir, &dst_dir);
    if (r < 0) {
        printf("cannot lookup destination directory %s: %s\n", dir, e2s(r));
        exit(-1);
    }

    struct fs_inode dst;
    r = fs_create(dst_dir, fn, &dst, l.to_ulabel());
    if (r < 0) {
        printf("cannot create destination file: %s\n", e2s(r));
        exit(-1);
    }
    
    fs_pwrite(dst, file_string, strlen(file_string), 0);   
}

static void
read(uint64_t taint)
{
    struct fs_inode fscat;
    error_check(fs_namei("/bin/cat", &fscat));

    label ds(3);
    ds.set(taint, LB_LEVEL_STAR);

    const char *argv[2] = { "/bin/cat", "/x/grptest.txt" };
    struct child_process cat = spawn(start_env->shared_container,
                                       fscat,
                                       0, 1, 2,
                                       2, &argv[0],
                                       0, &ds, 0, 0);
    int64_t e;
    process_wait(&cat, &e);    
}

static void
write(void)
{
    struct fs_inode grptest;
    error_check(fs_namei("/x/grptest.txt", &grptest));
    
    uint64_t off;
    if (fs_getsize(grptest, &off) < 0) {
        printf("unable to get file size\n");
        exit(-1);
    }
    
    if (fs_resize(grptest, off + strlen(file_string)) < 0) {
        printf("unable to resize file\n");
        exit(-1);
    }
    
    fs_pwrite(grptest, file_string, strlen(file_string), off);  
}

int
main (int ac, char **av)
{
    if (ac != 4)
        usage();
    
    const char *gname = av[1];
    const char *uname = av[2];
    uint64_t t, g;
    auth_unamehandles(uname, &t, &g); 
    authd_reply reply;

    if (auth_groupcall(authd_logingroup, 0, gname, t, g, &reply) < 0) {
        printf("error during group login\n");
        exit(-1);
    }
        
    label ds;
    thread_cur_label(&ds);
    
    if (!strcmp(av[3], "create"))
        create(reply.user_taint, reply.user_grant);    
    else if (!strcmp(av[3], "read"))
        read(reply.user_taint);
    else if (!strcmp(av[3], "write"))
        write();
    else
        usage();
    return 0;    
}
