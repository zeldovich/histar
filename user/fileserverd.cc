#include <lib/dis/fileserver.hh>

int
main (int ac, char **av)
{
    fileserver_start(8080);
    return 0;    
}
