#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <inc/lib.h>

libc_hidden_proto(getuid)
libc_hidden_proto(geteuid)
libc_hidden_proto(getgid)
libc_hidden_proto(getegid)
libc_hidden_proto(getgroups)
libc_hidden_proto(setuid)
libc_hidden_proto(seteuid)
libc_hidden_proto(setgid)
libc_hidden_proto(setegid)
libc_hidden_proto(setgroups)

uid_t
getuid()
{
    return start_env->uid;
}

gid_t
getgid()
{
    return start_env->gid;
}

uid_t
geteuid()
{
    return start_env->uid;
}

gid_t
getegid()
{
    return start_env->gid;
}

int
setuid(uid_t uid)
{
    start_env->uid = uid;
    return 0;
}

int
seteuid(uid_t uid)
{
    start_env->uid = uid;
    return 0;
}

int
setgid(gid_t gid)
{
    return 0;
}

int
setegid(gid_t gid)
{
    return 0;
}

int
setreuid(uid_t ruid, uid_t euid)
{
    start_env->uid = ruid;
    return 0;
}

int
setregid(gid_t rgid, gid_t egid)
{
    start_env->gid = rgid;
    return 0;
}

int
setgroups(size_t size, const gid_t *list)
{
    return 0;
}

int getgroups(int size, gid_t *list)
{
    return 0;    
}

libc_hidden_def(getuid)
libc_hidden_def(geteuid)
libc_hidden_def(getgid)
libc_hidden_def(getegid)
libc_hidden_def(getgroups)
libc_hidden_def(setuid)
libc_hidden_def(seteuid)
libc_hidden_def(setgid)
libc_hidden_def(setegid)
libc_hidden_def(setgroups)

