extern "C" {
#include <inc/lib.h>
#include <inc/authd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
}

#include <inc/error.hh>
#include <inc/spawn.hh>

int
main (int ac, char **av)
{
    struct fs_inode fs_share_fetch;
    error_check(fs_namei("/bin/fetch", &fs_share_fetch));
    const char *argv[3] = { "/bin/fetch", 
			    "http://171.67.22.26/~silasb/share.tar",
			    "/x/share.tar"};
    struct child_process share_fetch = spawn(start_env->shared_container,
                                       fs_share_fetch,
                                       0, 1, 2,
                                       3, &argv[0],
                                       0, 0, 0, 0);
    int64_t e;
    process_wait(&share_fetch, &e);
    ///////
    struct fs_inode fs_share_tar;
    error_check(fs_namei("/bin/tar", &fs_share_tar));
    const char *argv2[5] = { "/bin/tar", 
			    "xf",
			    "/x/share.tar",
			    "-C",
			    "/x"};
    struct child_process share_tar = spawn(start_env->shared_container,
                                       fs_share_tar,
                                       0, 1, 2,
                                       5, &argv2[0],
                                       0, 0, 0, 0);
    process_wait(&share_tar, &e);
    return 0;
}
