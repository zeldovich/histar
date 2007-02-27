#ifndef JOS_HTTPD_INC_A2PDF_HH
#define JOS_HTTPD_INC_A2PDF_HH

#include <iostream>

void a2pdf(fs_inode root_ino, const char *fn, 
	   uint64_t utaint, std::ostringstream &out); 

#endif
