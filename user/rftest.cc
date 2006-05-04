extern "C" {
#include <inc/lib.h>    
#include <inc/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int remfile_open(char *host, char *path);
}

#include <inc/labelutil.hh>

#include <lib/dis/proxydclnt.hh>

int
main (int ac, char **av)
{
    char buf[16];
    
    int fd = remfile_open((char*)"1",(char*)"2");
    int count = read(fd, buf, 16);    
    printf("count %d\n", count);
    count = write(fd, buf, 16);    
    printf("w count %d\n", count);
    
    return 0;    
}
