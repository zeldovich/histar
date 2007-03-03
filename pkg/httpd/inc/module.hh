#ifndef JOS_HTTPD_INC_MODULE_HH
#define JOS_HTTPD_INC_MODULE_HH

#include <iostream>

void perl(fs_inode root_ino, const char *fn, 
	  uint64_t utaint, uint64_t ugrant, std::ostringstream &out);
void a2pdf(fs_inode root_ino, const char *fn, 
	   uint64_t utaint, uint64_t ugrant, std::ostringstream &out); 
void cat(fs_inode root_ino, const char *fn, 
	 uint64_t utaint, uint64_t ugrant, std::ostringstream &out);

#endif
