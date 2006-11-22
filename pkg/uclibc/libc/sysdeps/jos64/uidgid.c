#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>
#include <inc/lib.h>

uid_t
getuid()
{
    return start_env->ruid;
}

gid_t
getgid()
{
    return 0;
}

uid_t
geteuid()
{
    return start_env->euid;
}

gid_t
getegid()
{
    return 0;
}

int
setuid(uid_t uid)
{
    if (start_env->euid == 0)
	start_env->ruid = uid;

    start_env->euid = uid;
    return 0;
}

int
seteuid(uid_t uid)
{
    start_env->euid = uid;
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
    start_env->ruid = ruid;
    start_env->euid = euid;
    return 0;
}

int
setregid(gid_t rgid, gid_t egid)
{
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
