#include <sfslite/async.h>
#define FD_MAX 64
#define NCON_MAX FD_MAX - 8
#define FINGER_PORT 79
char **nexttarget;
char **lasttarget;
void launchmore ();
struct finger {
    static int ncon;
    const str user;
    const str host;
    int fd;
    strbuf buf;
    static void launch (const char *target);
    finger (str u, str h);
    ~finger ();
    void connected (int f);
    void senduser ();
    void recvreply ();
};
int finger::ncon;

void
finger::launch (const char *target)
{
    if (const char *at = strrchr(target, '@')) {
	str user (target, at - target);
	str host (at + 1);
	vNew finger (user, host);
    }
    else
	warn << target << ": could not parse finger target\n";
}
finger::finger (str u, str h)
    : user (u), host (h), fd (-1)
{
    ncon++;
    buf << user << "\r\n";
    tcpconnect (host, FINGER_PORT, wrap (this, &finger::connected));
}
void
finger::connected (int f)
{
    fd = f;
    if (fd < 0) {
	warn << host << ": " << strerror (errno) << "\n";
	delete this;
	return;
    }
    fdcb (fd, selwrite, wrap (this, &finger::senduser));
}
void
finger::senduser ()
{
    if (buf.tosuio ()->output (fd) < 0) {
	warn << host << ": " << strerror (errno) << "\n";
	delete this;
	return;
    }
    if (!buf.tosuio ()->resid ()) {
	buf << "[" << user << "@" << host << "]\n";
	fdcb (fd, selwrite, NULL);
	fdcb (fd, selread, wrap (this, &finger::recvreply));
    }
}

void
finger::recvreply ()
{
    switch (buf.tosuio ()->input (fd)) {
    case -1:
	if (errno != EAGAIN) {
	    warn << host << ": " << strerror (errno) << "\n";
	    delete this;
	}
	break;
    case 0:
	buf.tosuio ()->output (1);
	delete this;
	break;
    }
}

finger::~finger ()
{
    if (fd >= 0) {
	fdcb (fd, selread, NULL);
	fdcb (fd, selwrite, NULL);
	close (fd);
    }
    ncon--;
    launchmore ();
}
void
launchmore ()
{
    while (nexttarget < lasttarget && finger::ncon < NCON_MAX)
	finger::launch (*nexttarget++);
    if (nexttarget == lasttarget && !finger::ncon)
	exit (0);
}
int
main (int argc, char **argv)
{
    make_sync (1);
    nexttarget = argv + 1;
    lasttarget = argv + argc;
    launchmore ();
    amain ();
}
