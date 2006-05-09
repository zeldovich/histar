#ifndef JOS_INC_FILESERVER_UTIL_HH_
#define JOS_INC_FILESERVER_UTIL_HH_

// put jos code (labels, jos fs, etc) in here...
class global_label;

void fileserver_acquire(char *path, int mode);
global_label *fileserver_new_global(char *path);


#endif /*JOS_INC_FILESERVER_UTIL_HH_*/
