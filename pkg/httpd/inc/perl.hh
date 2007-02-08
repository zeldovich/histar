#ifndef JOS_HTTPD_INC_PERL_HH
#define JOS_HTTPD_INC_PERL_HH

#include <iostream>
#include <inc/wrap.hh>

void perl(const char *fn, uint64_t utaint, std::ostringstream &out);

#endif
