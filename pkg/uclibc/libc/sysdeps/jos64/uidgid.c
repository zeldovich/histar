#include <unistd.h>
#include <grp.h>
#include <sys/types.h>

uid_t
getuid()
{
    return 0;
}

gid_t
getgid()
{
    return 0;
}

uid_t
geteuid()
{
    return 0;
}

gid_t
getegid()
{
    return 0;
}

int
setuid(uid_t uid)
{
    return 0;
}

int
seteuid(uid_t uid)
{
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
