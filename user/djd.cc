#include <async.h>
#include <dis.hh>

int
main(int ac, char **av)
{
    uint16_t port = 5923;
    warn << "instantiating a djserv, port " << port << "...\n";
    ptr<djserv> djs = djserv::alloc(port);
    amain();
}
