#include <async.h>
#include <dj/dis.hh>

int
main(int ac, char **av)
{
    uint16_t port = 5923;
    warn << "instantiating a djprot, port " << port << "...\n";
    ptr<djprot> djs = djprot::alloc(port);
    amain();
}
