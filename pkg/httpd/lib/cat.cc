extern "C" {
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

#include <unistd.h>
}

#include <iostream>
#include <sstream>

#include <inc/module.hh>
#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/spawn.hh>
#include <inc/wrap.hh>

static const char debug = 1;

void
webcat(fs_inode root_ino, const char *fn, uint64_t utaint, uint64_t ugrant, std::ostringstream &out)
{
    const char *av[] = { "/bin/cat", fn };

    struct stat sb;
    if (stat(fn, &sb) < 0) {
	out << "HTTP/1.0 404 Not Found\r\n";
	out << "Content-Type: text/html\r\n";
	out << "\r\n";
	out << "Error cating file " << fn << ": " << strerror(errno) << "\r\n";
	return;
    } 

    out << "HTTP/2.0 200 OK\r\n";
    out << "Content-Type: text/html\r\n";
    out << "\r\n";

    label taint(0);
    if (utaint)
	taint.set(utaint, 3);

    label grant(3);
    if (ugrant)
	grant.set(ugrant, LB_LEVEL_STAR);
    
    wrap_call wc("/bin/cat", root_ino);
    wc.call(2, av, 0, 0, &taint, &grant, out);
    
    int64_t exit_code;
    error_check(process_wait(wc.child_proc(), &exit_code));
    error_check(exit_code);    
}
