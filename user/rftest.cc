extern "C" {
#include <inc/lib.h>    
#include <inc/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

int remfile_open(char *host, int port, char *path);
}

#include <inc/labelutil.hh>

#include <lib/dis/proxydclnt.hh>

static void
usage(char *com)
{
    printf("usage: %s port [ip]\n", com);
    exit(-1);    
}

int
main (int ac, char **av)
{
    char buf[16];
    char foo[] = "hellloooo";
    int fd;
    
    if (ac < 2)
        usage(av[0]);
    
    int port = atoi(av[1]);
    
    if (ac < 3)
        fd = remfile_open((char*)"127.0.0.1", port, (char*)"test.txt");
    else 
        fd = remfile_open((char*)av[1], port, (char*)"test.txt");
    int count = read(fd, buf, 16);    
    printf("count %d ", count);
    printf("contents ");
    for (int i = 0; i < count; i++)
        printf("%c ", buf[i]);
    printf("\n");
    
    count = write(fd, foo, strlen(foo));    
    printf("w count %d\n", count);
    
    struct stat st;
    if (fstat(fd, &st) < 0)
        printf("fstat error!\n");
    else
        printf("size %ld\n", st.st_size);
    return 0;    
}
