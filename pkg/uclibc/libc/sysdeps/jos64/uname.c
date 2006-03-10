#include <sys/utsname.h>


// XXX
int 
uname (struct utsname *__name) __THROW
{
    __name->sysname[0] = '\0' ;
    __name->nodename[0] = '\0' ;
    __name->release[0] = '\0' ;
    __name->version[0] = '\0' ;
    __name->machine[0] = '\0' ;
    
    return 0 ;
}