extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   
}

#include <lib/dis/fileclient.hh>

static void 
usage(char *com)
{
    printf("usage: %s port\n", com);    
    exit(-1);
}

int
main (int ac, char **av)
{
    if (ac < 2)
        usage(av[0]);
    int port = atoi(av[1]);

    fileclient *fc = new fileclient("/x/test.txt", "127.0.0.1", port);
    const file_frame *frame = fc->frame_at(10, 0);
    printf("frame->count %ld frame->offset %ld\n", frame->count_, frame->offset_);

    return 0;    
}
