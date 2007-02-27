extern "C" {
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/stdio.h>

#include <unistd.h>
}

#include <iostream>
#include <sstream>

#include <inc/perl.hh>
#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <inc/spawn.hh>

static const char debug = 1;

void
perl(fs_inode root_ino, const char *fn, uint64_t utaint, std::ostringstream &out)
{
    const char *av[] = { "/bin/perl", fn };


    struct stat sb;
    if (stat(fn, &sb) < 0) {
	out << "HTTP/1.0 404 Not Found\r\n";
	out << "Content-Type: text/html\r\n";
	out << "\r\n";
	out << "Cannot find script " << fn << "\r\n";
	return;
    } 

    out << "HTTP/2.0 200 OK\r\n";

    int64_t ctaint;
    error_check(ctaint = handle_alloc());
    scope_guard<void, uint64_t> drop(thread_drop_star, ctaint);
    
    label taint(0);
    taint.set(utaint, 3);
    taint.set(ctaint, 3);
    
    wrap_call wc("/bin/perl", root_ino);
    wc.call(2, av, 0, 0, &taint, out);
    
    int64_t exit_code;
    error_check(process_wait(wc.child_proc(), &exit_code));
    error_check(exit_code);    
}
