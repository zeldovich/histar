#include <unistd.h>
#include <bits/unimpl.h>

libc_hidden_proto(getdomainname)

int 
getdomainname(char *name, size_t len)
{
    set_enosys();
    return -1;
}

libc_hidden_def(getdomainname)

