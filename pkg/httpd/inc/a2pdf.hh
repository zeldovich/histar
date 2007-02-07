#ifndef JOS_HTTPD_INC_A2PDF_HH
#define JOS_HTTPD_INC_A2PDF_HH

#include <iostream>

uint64_t a2pdf(int fd, std::ostringstream &pdf_out, uint64_t taint);

#endif
