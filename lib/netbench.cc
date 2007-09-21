extern "C" {
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
}

#include <inc/error.hh>
#include <inc/netbench.hh>

void
nb_read(int s, char *b, int count)
{
    while (count) {
	int r = read(s, b, count);
	if (r < 0)
	    throw basic_exception("read: %s", strerror(errno));
	if (r == 0)
	    throw basic_exception("read: EOF");
	count -= r;
	b += r;
    }
}

void
nb_write(int s, char *b, int count)
{
    while (count) {
	int r = write(s, b, count);
	if (r < 0)
	    throw basic_exception("write: %s", strerror(errno));
	if (r == 0)
	    throw basic_exception("write: unable to write");
	count -= r;
	b += r;
    }
}
