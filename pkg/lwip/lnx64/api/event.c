#include <malloc.h>

#include <api/ext.h>

extern struct lwip_socket *sockets;

void
lwipext_init(char public_sockets)
{
    uint64_t bytes = NUM_SOCKETS * sizeof(struct lwip_socket);
    sockets = malloc(bytes);
}
