#ifndef JOS_HTTPD_INC_PERL_HH
#define JOS_HTTPD_INC_PERL_HH

#include <iostream>

uint64_t perl(const char *fn, std::ostringstream &perl_out, uint64_t taint);

#endif
