#ifndef JOS_HTTPD_INC_MODULE_HH
#define JOS_HTTPD_INC_MODULE_HH

extern "C" {
#include <inc/fs.h>
}

#include <sstream>

void perl(fs_inode root_ino, const char *fn, 
	  uint64_t utaint, uint64_t ugrant, std::ostringstream &out);
void a2pdf(fs_inode root_ino, const char *fn, 
	   uint64_t utaint, uint64_t ugrant, std::ostringstream &out); 
void webcat(fs_inode root_ino, const char *fn, 
	    uint64_t utaint, uint64_t ugrant, std::ostringstream &out);

#endif
