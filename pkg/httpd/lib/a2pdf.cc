extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/fd.h>
}

#include <iostream>
#include <sstream>


#include <inc/module.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/errno.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/wrap.hh>

void 
a2pdf(fs_inode root_ino, const char *fn, uint64_t utaint, uint64_t ugrant, std::ostringstream &out)
{
    const char *av0[] = { "/bin/a2ps",
			  "--quiet",
			  "--output=-",
			  "--pretty-print=plain",
			  fn };
    const char *av1[] = { "/bin/gs", 
			  "-q",     
			  "-dNOPAUSE", 
			  "-dBATCH", 
			  "-sDEVICE=pdfwrite", 
			  "-sOutputFile=-", 
			  "-c",
			  ".setpdfwrite",
			  "-f",
			  "-" };
    
    label taint(0);
    if (utaint)
	taint.set(utaint, 3);

    label grant(3);
    if (ugrant)
	grant.set(ugrant, LB_LEVEL_STAR);

    std::ostringstream pdf_out;

    wrap_call wc0("/bin/a2ps", root_ino);
    wrap_call wc1("/bin/gs", root_ino);
    wc0.call(5, av0, 0, 0, &taint, &grant);
    wc0.pipe(&wc1, 10, av1, 0, 0, &taint, &grant, pdf_out);

    int64_t exit_code0, exit_code1;
    error_check(process_wait(wc0.child_proc(), &exit_code0));
    error_check(exit_code0);    

    error_check(process_wait(wc1.child_proc(), &exit_code1));
    error_check(exit_code1);    

    std::string pdf = pdf_out.str();
    char size[32];
    sprintf(size, "%ld", pdf.size());
    std::string content_length = std::string("Content-Length: ") + size + "\r\n";
	
    out << "HTTP/2.0 200 OK\r\n";
    out << "Content-Type: application/pdf\r\n";
    out << content_length;
    out << "\r\n";
    
    out << pdf;

    return;
}
