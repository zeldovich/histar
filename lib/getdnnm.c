#include <unistd.h>
#include <bits/unimpl.h>

int 
getdomainname(char *name, size_t len)
{
    set_enosys();
    return -1;
}
